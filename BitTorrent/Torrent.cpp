#include <bitset>
#include <iostream>
#include <vector>
#include <any>
#include <string>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <functional>
#include <iomanip>
#include <array>
#include <cmath>

namespace BitTorrent {

    class Torrent;  // Forward declaration

    class FileItem {
    public:
        std::string Path;
        long Size;
        long Offset;
        std::string FormattedSize;

        // Fix naming conflict and use a proper function definition
        std::string GetFormattedSize(long size) {
            return Torrent::BytesToString(size);
        }
    };

    class Tracker {
    public:
        std::function<void(const std::vector<std::string>&)> PeerListUpdated;
        std::string Address;

        explicit Tracker(const std::string& address) : Address(address) {}

        void UpdatePeers(const std::vector<std::string>& newPeers) {
            if (PeerListUpdated) {  // Ensure a function is assigned before calling
                PeerListUpdated(newPeers);
            }
        }
    };

    class Torrent {
    public:
        std::string Name;
        bool IsPrivate;
        std::vector<FileItem> Files;
        std::string FilePath;
        std::string DownloadDirectory;
        std::vector<Tracker> Trackers;
        std::string Comment;
        std::string CreatedBy;
        time_t CreationDate;
        std::string Encoding;
        int BlockSize;
        int PieceSize;
        std::vector<std::vector<uint8_t>> PieceHashes;
        std::array<uint8_t, 20> Infohash{};  // Equivalent to `new byte[20]` in C#
        long TotalSize() const {
            long total = 0;
            for (const auto& file : Files) {
                total += file.Size;
            }
            return total;
        }
        std::string FormattedPieceSize() const {
            return BytesToString(PieceSize);
        }
        std::string FormattedTotalSize() const {
            return BytesToString(TotalSize());
        }
        int PieceCount() const {
            return static_cast<int>(PieceHashes.size());
        }

        std::vector<std::vector<uint8_t>> PieceHashes;
        std::vector<std::vector<bool>> IsBlockAcquired;
        std::vector<bool> IsPieceVerified;

        std::string VerfiesPiecesString() const {
            
            std::string str;
            for (auto piece : IsPieceVerified) {
                str += piece ? "1" : "0";
            }
            return str;
        }

        int VerifiedPiecesCount() const {
            return std::count(IsPieceVerified.begin(), IsPieceVerified.end(), true);
        }

        double VerifiedRatio() const {
            return VerifiedPiecesCount() / static_cast<double>(PieceCount());
        }
        bool Iscompleted() const {
            return VerifiedPiecesCount() == PieceCount();
        }
        bool IsStarted() const {
            return VerifiedPiecesCount() > 0;
        }
        long Uploaded = 0;
        long Downloaded() const{
            return PieceSize*VerifiedPiecesCount();
        }
        long Left() const {
            return TotalSize() - Downloaded();
        }                                                  
        int GetPieceSize(int piece){
            if (piece == PieceCount() - 1) {
                return TotalSize() % PieceSize;
            } else {
                return PieceSize;
            }
        }

        int GetBlockSize(int piece, int block) {
            if (piece == PieceCount() - 1 && block == PieceCount() - 1) {
                return TotalSize() % BlockSize;
            } else {
                return BlockSize;
            }
        }

        int GetBlockCount(int piece) {
            if (piece == PieceCount() - 1) {
                return (TotalSize() % PieceSize) / BlockSize + 1;
            } else {
                return PieceSize / BlockSize;
            }
        }

        std::vector<std::any> fileWriteLocks;
        static std::SHA1 sha1 = SHA1.create();
        
        Torrent(std::string name, std::string location, std::vector<FileItem> files, std::vector<std::string> trackers, int pieceSize, int blockSize, std::array<uint8_t, 20> infohash, int blocksize = 161384, bool isPrivate = false){
            Name = name;
            DownloadDirectory = location;
            Files = files;
            fileWriteLocks = std::vector<std::any>(files.size(), nullptr);
            for (std::string url : trackers) {
                Tracker tracker = Tracker(url);
                Trackers.push_back(tracker);
                tracker.PeerListUpdated += HandlepeerListUpdated;
            }
            PieceSize = pieceSize;
            BlockSize = blockSize;
            IsPrivate = isPrivate;
            int count = static_cast<int>(std::ceil(TotalSize() / static_cast<double>(PieceSize)));
            PieceHashes.resize(count, std::vector<uint8_t>(20));
            IsPieceVerified.resize(count, false);
            IsBlockAcquired.resize(count);

            for (int i = 0; i < PieceCount(); i++) {
                IsBlockAcquired[i].resize(GetBlockCount(i), false);
            }

            if (PieceHashes.empty()) {
                // This is a new torrent, so we have to create the hashes from the files                 
                for (int i = 0; i < PieceCount(); i++) {
                    PieceHashes[i] = GetHash(i);
                }
            }

            std::ostringstream infoStream;
            TorrentInfoToBEncodingObject(infoStream);  // Define TorrentInfoToBEncodingObject elsewhere
            std::string info = infoStream.str();
            std::vector<uint8_t> bytes(info.begin(), info.end());
            Infohash = ComputeSHA1Hash(bytes);  // Define ComputeSHA1Hash elsewhere

            for (int i = 0; i < PieceCount(); i++) {
                Verify(i);  // Verify the piece at index i
            }

        }

        // Define the GetHash function to compute a hash for a given piece index
        std::vector<uint8_t> GetHash(int pieceIndex) {
            // Example implementation: Generate a dummy hash for demonstration purposes
            std::vector<uint8_t> hash(20, static_cast<uint8_t>(pieceIndex % 256));
            return hash;
        }
        
        
           

        std::string GetFileDirectory() const {
            return Files.size() > 1 ? Name + FilePath : "";
        }

        // Convert bytes to a hexadecimal string
        std::string HexStringInfohash() const {
            std::ostringstream hexStream;
            for (uint8_t byte : Infohash) {
                hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            }
            return hexStream.str();
        }

        // Convert bytes to a URL-safe string (percent-encoded format)
        std::string UrlSafeStringInfohash() const {
            std::ostringstream encodedStream;
            for (uint8_t byte : Infohash) {
                if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
                    encodedStream << static_cast<char>(byte);
                } else {
                    encodedStream << '%' << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                }
            }
            return encodedStream.str();
        }

        // Define BytesToString to fix undefined reference issue
        static std::string BytesToString(long bytes) {
            std::ostringstream out;
            if (bytes < 1024) {
                out << bytes << " B";
            } else if (bytes < 1024 * 1024) {
                out << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB";
            } else if (bytes < 1024 * 1024 * 1024) {
                out << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
            } else {
                out << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
            }
            return out.str();
        }
        void Verify(int pieceIndex) {
            // Example implementation: Mark the piece as verified if all blocks are acquired
            if (std::all_of(IsBlockAcquired[pieceIndex].begin(), IsBlockAcquired[pieceIndex].end(), [](bool acquired) { return acquired; })) {
                IsPieceVerified[pieceIndex] = true;
            } else {
                IsPieceVerified[pieceIndex] = false;
            }
        }
    };
}
