#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

// =============================================================================
// WdfArchive — Read-only access to a single .wdf archive file
//
// Opens a WDF archive, loads its index into memory (sorted by UID for binary
// search), and provides entry lookup and raw byte reads.
//
// Confidence labels:
//   [Confirmed]           — directly verified from reference source code
//   [Partially Confirmed] — visible in reference; semantic uncertain
//   [Unknown]             — no reference evidence; working assumption adopted
//   [Adopted — verify]    — working interpretation; test against real data
// =============================================================================

namespace LongXi
{

// WDF archive header — 12 bytes at offset 0 in the file.
struct WdfHeader
{
    uint32_t Id;     // [Partially Confirmed — field present; semantic unknown; NOT validated]
    int32_t Count;   // [Confirmed — number of entries in the index table]
    uint32_t Offset; // [Confirmed — byte offset from file start to the index table]
};

// WDF index entry — 16 bytes per entry, at WdfHeader::Offset.
struct WdfIndexEntry
{
    uint32_t Uid;    // [Confirmed — 32-bit hash of lowercase forward-slash-normalized path]
    uint32_t Offset; // [Confirmed — byte offset of entry data from file start]
    uint32_t Size;   // [Confirmed — byte size of entry data]
    uint32_t Space;  // [Unknown — present in file; semantic unknown; NOT used in reads]
};

// ============================================================================
// WdfArchive
// ============================================================================
class WdfArchive
{
  public:
    WdfArchive();
    ~WdfArchive();

    WdfArchive(const WdfArchive&) = delete;
    WdfArchive& operator=(const WdfArchive&) = delete;

    // Open a .wdf archive at the given absolute path (UTF-8, forward-slash).
    // Reads header and full index. Sorts index if not pre-sorted (logs WARN).
    // Returns false on any failure (logged).
    bool Open(const std::string& absolutePath);

    void Close();
    bool IsOpen() const;
    const std::string& GetPath() const;
    int GetEntryCount() const;

    bool HasEntry(const std::string& normalizedPath) const;

    // Returns nullopt if entry not found or read fails (logged).
    std::optional<std::vector<uint8_t>> ReadEntry(const std::string& normalizedPath) const;

  private:
    // [Adopted — verify] hash reimplemented from x86 assembly reference
    uint32_t ComputeUid(const std::string& path) const;

  private:
    std::string m_Path;
    mutable std::ifstream m_File;       // mutable — seekg/read are non-const
    std::vector<WdfIndexEntry> m_Index; // sorted by Uid after Open()
    bool m_IsOpen;
};

} // namespace LongXi
