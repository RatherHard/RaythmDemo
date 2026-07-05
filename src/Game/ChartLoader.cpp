// RaythmDemo - Game Chart Loader Implementation
// Implements JSON chart parsing and file loading for the Game module.
// Author: RatherHard
// Date: 2026-07-05

#include "Game/ChartLoader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Raythm::Game
{
    namespace
    {
        using Json = nlohmann::json;

        /** @brief Maximum beat tuple component accepted by this minimal loader. */
        constexpr std::int64_t MAX_BEAT_TUPLE_COMPONENT = 1'000'000;

        /** @brief Canonical zero beat position used for required first BPM validation. */
        constexpr BeatPosition ZERO_BEAT{};

        /**
         * @brief Formats an invalid chart message with a consistent prefix.
         * @param message Field-specific validation message.
         * @return Runtime error containing chart context.
         * @note Keeps loader call sites concise while producing actionable test failures.
         */
        std::runtime_error chartError(const std::string& message)
        {
            return std::runtime_error("Invalid chart: " + message);
        }

        /**
         * @brief Ensures a JSON object contains a required field.
         * @param object Object to inspect.
         * @param field Required field name.
         * @param context Parent context used in error messages.
         * @throws std::runtime_error when the field is absent.
         * @note Used before type-specific accessors to keep errors precise.
         */
        void requireField(const Json& object, const char* field, const std::string& context)
        {
            if (!object.contains(field))
            {
                throw chartError(context + "." + field + " is required");
            }
        }

        /**
         * @brief Reads a required string field from a JSON object.
         * @param object Object containing the field.
         * @param field Field name to read.
         * @param context Parent context used in error messages.
         * @return String value for the field.
         * @throws std::runtime_error when the field is missing or not a string.
         * @note Does not trim or normalize user-authored metadata.
         */
        std::string readStringField(const Json& object, const char* field, const std::string& context)
        {
            requireField(object, field, context);
            const Json& value = object.at(field);
            if (!value.is_string())
            {
                throw chartError(context + "." + field + " must be a string");
            }

            return value.get<std::string>();
        }

        /**
         * @brief Reads a required integer field from a JSON object.
         * @param object Object containing the field.
         * @param field Field name to read.
         * @param context Parent context used in error messages.
         * @return Integer value for the field.
         * @throws std::runtime_error when the field is missing or not an integer.
         * @note Uses nlohmann/json integer typing to reject floating point offsets and columns.
         */
        int readIntField(const Json& object, const char* field, const std::string& context)
        {
            requireField(object, field, context);
            const Json& value = object.at(field);
            if (!value.is_number_integer())
            {
                throw chartError(context + "." + field + " must be an integer");
            }

            try
            {
                return value.get<int>();
            }
            catch (const Json::exception& exception)
            {
                throw chartError(context + "." + field + " is outside the supported integer range: " + exception.what());
            }
        }

        /**
         * @brief Reads a chart beat tuple from JSON.
         * @param value JSON value expected to contain [x, y, z].
         * @param context Field context for errors.
         * @return Canonical beat position.
         * @throws std::runtime_error when the tuple shape or values are invalid.
         * @note Equivalent tuple forms are reduced through BeatPosition::fromTuple.
         */
        BeatPosition readBeat(const Json& value, const std::string& context)
        {
            if (!value.is_array() || value.size() != 3U)
            {
                throw chartError(context + " must be a three-integer beat array");
            }

            for (std::size_t index = 0; index < 3U; ++index)
            {
                if (!value[index].is_number_integer())
                {
                    throw chartError(context + " entries must be integers");
                }
            }

            try
            {
                return BeatPosition::fromTuple(
                    value[0].get<std::int64_t>(),
                    value[1].get<std::int64_t>(),
                    value[2].get<std::int64_t>());
            }
            catch (const Json::exception& exception)
            {
                throw chartError(context + " entries are outside the supported integer range: " + exception.what());
            }
            catch (const std::runtime_error& exception)
            {
                throw chartError(context + " is invalid: " + exception.what());
            }
        }

        /**
         * @brief Validates chart-declared audio path metadata.
         * @param audioPath UTF-8 relative path declared by meta.path.
         * @throws std::runtime_error when the path is absolute or contains parent traversal.
         * @note Actual asset existence and root resolution remain responsibilities of the later resource loader.
         */
        void validateMetadataAudioPath(const std::string& audioPath)
        {
            const std::filesystem::path path = std::filesystem::u8path(audioPath);
            if (audioPath.empty())
            {
                throw chartError("meta.path must not be empty");
            }
            if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
            {
                throw chartError("meta.path must be a relative resource path");
            }
            for (const std::filesystem::path& part : path)
            {
                if (part == "..")
                {
                    throw chartError("meta.path must not contain parent directory traversal");
                }
            }
        }

        /**
         * @brief Parses the metadata object from the chart root.
         * @param metaObject JSON metadata object.
         * @return Runtime metadata value.
         * @throws std::runtime_error when required metadata fields are invalid.
         * @note Resource-root validation is intentionally left outside this parser.
         */
        ChartMetadata parseMetadata(const Json& metaObject)
        {
            if (!metaObject.is_object())
            {
                throw chartError("meta must be an object");
            }

            ChartMetadata metadata{};
            metadata.title = readStringField(metaObject, "title", "meta");
            metadata.artist = readStringField(metaObject, "artist", "meta");
            metadata.creator = readStringField(metaObject, "creator", "meta");
            metadata.audioPath = readStringField(metaObject, "path", "meta");
            validateMetadataAudioPath(metadata.audioPath);
            metadata.offsetMilliseconds = readIntField(metaObject, "offset", "meta");
            return metadata;
        }

        /**
         * @brief Parses and validates BPM timing points.
         * @param timeArray JSON time array.
         * @return Sorted timing points.
         * @throws std::runtime_error when BPM values, beat values, or duplicate beats are invalid.
         * @note Sorting happens after parsing so input order does not matter.
         */
        std::vector<TimingPoint> parseTimingPoints(const Json& timeArray)
        {
            if (!timeArray.is_array())
            {
                throw chartError("time must be an array");
            }
            if (timeArray.empty())
            {
                throw chartError("time must contain at least one BPM entry");
            }

            std::vector<TimingPoint> timingPoints;
            timingPoints.reserve(timeArray.size());
            for (std::size_t index = 0; index < timeArray.size(); ++index)
            {
                const Json& timingObject = timeArray[index];
                const std::string context = "time[" + std::to_string(index) + "]";
                if (!timingObject.is_object())
                {
                    throw chartError(context + " must be an object");
                }

                requireField(timingObject, "beat", context);
                requireField(timingObject, "bpm", context);
                if (!timingObject.at("bpm").is_number())
                {
                    throw chartError(context + ".bpm must be a number");
                }

                double bpm = 0.0;
                try
                {
                    bpm = timingObject.at("bpm").get<double>();
                }
                catch (const Json::exception& exception)
                {
                    throw chartError(context + ".bpm is outside the supported numeric range: " + exception.what());
                }
                if (!std::isfinite(bpm) || bpm <= 0.0)
                {
                    throw chartError(context + ".bpm must be finite and greater than zero");
                }

                timingPoints.push_back({readBeat(timingObject.at("beat"), context + ".beat"), bpm});
            }

            std::sort(
                timingPoints.begin(),
                timingPoints.end(),
                [](const TimingPoint& lhs, const TimingPoint& rhs)
                {
                    return lhs.beat < rhs.beat;
                });

            if (!(timingPoints.front().beat == ZERO_BEAT))
            {
                throw chartError("first BPM entry must start at beat [0, 0, 1]");
            }

            for (std::size_t index = 1; index < timingPoints.size(); ++index)
            {
                if (timingPoints[index - 1].beat == timingPoints[index].beat)
                {
                    throw chartError("duplicate BPM entries at the same normalized beat are not allowed");
                }
            }

            return timingPoints;
        }

        /**
         * @brief Parses and validates note entries.
         * @param noteArray JSON note array.
         * @param laneCount Number of accepted note columns.
         * @return Sorted tap and hold notes.
         * @throws std::runtime_error when note fields are invalid.
         * @note Hold notes are preserved but not judged in this slice.
         */
        std::vector<Note> parseNotes(const Json& noteArray, int laneCount)
        {
            if (!noteArray.is_array())
            {
                throw chartError("note must be an array");
            }

            std::vector<Note> notes;
            notes.reserve(noteArray.size());
            for (std::size_t index = 0; index < noteArray.size(); ++index)
            {
                const Json& noteObject = noteArray[index];
                const std::string context = "note[" + std::to_string(index) + "]";
                if (!noteObject.is_object())
                {
                    throw chartError(context + " must be an object");
                }

                requireField(noteObject, "beat", context);
                const BeatPosition beat = readBeat(noteObject.at("beat"), context + ".beat");
                const int column = readIntField(noteObject, "column", context);
                if (column < 0 || column >= laneCount)
                {
                    throw chartError(context + ".column must be in range [0, " + std::to_string(laneCount - 1) + "]");
                }

                Note note{};
                note.beat = beat;
                note.column = column;
                if (noteObject.contains("endbeat"))
                {
                    note.endBeat = readBeat(noteObject.at("endbeat"), context + ".endbeat");
                    if (!(note.beat < *note.endBeat))
                    {
                        throw chartError(context + ".endbeat must be greater than beat");
                    }
                }

                notes.push_back(note);
            }

            std::sort(
                notes.begin(),
                notes.end(),
                [](const Note& lhs, const Note& rhs)
                {
                    if (lhs.beat == rhs.beat)
                    {
                        if (lhs.column == rhs.column)
                        {
                            return lhs.endBeat < rhs.endBeat;
                        }
                        return lhs.column < rhs.column;
                    }
                    return lhs.beat < rhs.beat;
                });
            return notes;
        }
        /**
         * @brief Reports whether a candidate path stays within a configured root.
         * @param root Canonical asset root.
         * @param candidate Canonical file path to inspect.
         * @return True when candidate is equal to or nested inside root.
         * @note Both inputs must be canonical paths for traversal and symlink checks to be meaningful.
         */
        bool isPathInsideRoot(const std::filesystem::path& root, const std::filesystem::path& candidate)
        {
            auto rootIterator = root.begin();
            auto candidateIterator = candidate.begin();
            while (rootIterator != root.end() && candidateIterator != candidate.end())
            {
                if (*rootIterator != *candidateIterator)
                {
                    return false;
                }

                ++rootIterator;
                ++candidateIterator;
            }

            return rootIterator == root.end();
        }

        /**
         * @brief Resolves the asset root used by chart file loading.
         * @param root Configured root path, or empty to use the current working directory.
         * @return Canonical directory path.
         * @throws std::runtime_error when the root cannot be resolved to a directory.
         * @note Canonicalization gives loadFromFile a stable confinement boundary.
         */
        std::filesystem::path resolveAssetRoot(const std::filesystem::path& root)
        {
            std::error_code error;
            std::filesystem::path canonicalRoot;
            try
            {
                const std::filesystem::path selectedRoot = root.empty() ? std::filesystem::current_path(error) : root;
                if (error)
                {
                    throw std::runtime_error("Invalid chart loader options: assetRoot must resolve to an existing directory");
                }

                canonicalRoot = std::filesystem::canonical(selectedRoot, error);
            }
            catch (const std::filesystem::filesystem_error& exception)
            {
                throw std::runtime_error(std::string("Invalid chart loader options: ") + exception.what());
            }
            if (error || !std::filesystem::is_directory(canonicalRoot, error) || error)
            {
                throw std::runtime_error("Invalid chart loader options: assetRoot must resolve to an existing directory");
            }

            return canonicalRoot;
        }

        /**
         * @brief Resolves and validates a chart file path under an asset root.
         * @param assetRoot Canonical asset root.
         * @param filePath Requested file path.
         * @return Canonical file path that is inside assetRoot.
         * @throws std::runtime_error when the path escapes the root or is not a regular file.
         * @note Canonical target resolution prevents traversal and symlink escapes.
         */
        std::filesystem::path resolveChartFilePath(const std::filesystem::path& assetRoot, const std::filesystem::path& filePath)
        {
            const std::filesystem::path rootedPath = filePath.is_absolute() ? filePath : assetRoot / filePath;

            std::error_code error;
            const std::filesystem::path canonicalPath = std::filesystem::canonical(rootedPath, error);
            if (error)
            {
                throw std::runtime_error("unable to resolve file path");
            }
            if (!isPathInsideRoot(assetRoot, canonicalPath))
            {
                throw std::runtime_error("file path escapes the configured asset root");
            }
            if (!std::filesystem::is_regular_file(canonicalPath, error) || error)
            {
                throw std::runtime_error("path is not a regular file");
            }

            return canonicalPath;
        }
        /**
         * @brief Reads a regular chart file from an already validated path without following final symlinks.
         * @param assetRoot Canonical asset root used for post-open confinement checks.
         * @param filePath Canonical path selected by resolveChartFilePath.
         * @param maxFileSizeBytes Maximum allowed file size in bytes.
         * @return Complete file contents.
         * @throws std::runtime_error when the opened handle is unsafe or cannot be read.
         * @note The handle is opened and then size/type-checked to avoid path-swap races between validation and read.
         */
        std::string readRegularFileNoFollow(
            const std::filesystem::path& assetRoot,
            const std::filesystem::path& filePath,
            std::uintmax_t maxFileSizeBytes)
        {
#if defined(_WIN32)
            struct FileHandle
            {
                HANDLE handle = INVALID_HANDLE_VALUE;

                ~FileHandle()
                {
                    if (handle != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(handle);
                    }
                }
            };

            FileHandle file{
                CreateFileW(
                    filePath.wstring().c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                    nullptr)};
            if (file.handle == INVALID_HANDLE_VALUE)
            {
                throw std::runtime_error("unable to open file handle");
            }
            if (GetFileType(file.handle) != FILE_TYPE_DISK)
            {
                throw std::runtime_error("opened path is not a disk file");
            }

            BY_HANDLE_FILE_INFORMATION fileInformation{};
            if (GetFileInformationByHandle(file.handle, &fileInformation) == 0)
            {
                throw std::runtime_error("unable to inspect opened file");
            }
            if ((fileInformation.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                throw std::runtime_error("symbolic links and reparse points are not accepted");
            }
            if ((fileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                throw std::runtime_error("opened path is not a regular file");
            }

            std::wstring finalPathBuffer(MAX_PATH * 4U, L'\0');
            const DWORD finalPathLength = GetFinalPathNameByHandleW(
                file.handle,
                finalPathBuffer.data(),
                static_cast<DWORD>(finalPathBuffer.size()),
                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (finalPathLength == 0 || finalPathLength >= finalPathBuffer.size())
            {
                throw std::runtime_error("unable to verify opened file path");
            }
            finalPathBuffer.resize(finalPathLength);
            constexpr std::wstring_view WINDOWS_LONG_PATH_PREFIX = L"\\\\?\\";
            constexpr std::wstring_view WINDOWS_LONG_UNC_PREFIX = L"\\\\?\\UNC\\";
            if (finalPathBuffer.rfind(WINDOWS_LONG_UNC_PREFIX, 0) == 0)
            {
                finalPathBuffer.replace(0, WINDOWS_LONG_UNC_PREFIX.size(), L"\\\\");
            }
            else if (finalPathBuffer.rfind(WINDOWS_LONG_PATH_PREFIX, 0) == 0)
            {
                finalPathBuffer.erase(0, WINDOWS_LONG_PATH_PREFIX.size());
            }
            if (!isPathInsideRoot(assetRoot, std::filesystem::weakly_canonical(std::filesystem::path(finalPathBuffer))))
            {
                throw std::runtime_error("opened file escapes the configured asset root");
            }

            LARGE_INTEGER fileSize{};
            if (GetFileSizeEx(file.handle, &fileSize) == 0 || fileSize.QuadPart < 0)
            {
                throw std::runtime_error("unable to inspect opened file size");
            }
            if (static_cast<std::uintmax_t>(fileSize.QuadPart) > maxFileSizeBytes)
            {
                throw std::runtime_error("file exceeds the configured chart size limit");
            }

            std::string contents;
            contents.resize(static_cast<std::size_t>(fileSize.QuadPart));
            std::size_t offset = 0;
            while (offset < contents.size())
            {
                const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(
                    contents.size() - offset,
                    static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
                DWORD bytesRead = 0;
                if (ReadFile(file.handle, contents.data() + offset, chunkSize, &bytesRead, nullptr) == 0)
                {
                    throw std::runtime_error("unable to read opened file");
                }
                if (bytesRead == 0)
                {
                    throw std::runtime_error("opened file ended before expected size");
                }

                offset += bytesRead;
            }

            return contents;
#elif defined(__linux__)
            struct FileDescriptor
            {
                int descriptor = -1;

                ~FileDescriptor()
                {
                    if (descriptor >= 0)
                    {
                        close(descriptor);
                    }
                }
            };

            FileDescriptor file{open(filePath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
            if (file.descriptor < 0)
            {
                throw std::runtime_error(std::string("unable to open file handle: ") + std::strerror(errno));
            }

            struct stat fileStat{};
            if (fstat(file.descriptor, &fileStat) != 0)
            {
                throw std::runtime_error(std::string("unable to inspect opened file: ") + std::strerror(errno));
            }
            if (!S_ISREG(fileStat.st_mode))
            {
                throw std::runtime_error("opened path is not a regular file");
            }
            if (fileStat.st_size < 0 || static_cast<std::uintmax_t>(fileStat.st_size) > maxFileSizeBytes)
            {
                throw std::runtime_error("file exceeds the configured chart size limit");
            }

            const std::filesystem::path descriptorPath = "/proc/self/fd/" + std::to_string(file.descriptor);
            std::error_code finalPathError;
            const std::filesystem::path finalPath = std::filesystem::canonical(descriptorPath, finalPathError);
            if (finalPathError || !isPathInsideRoot(assetRoot, finalPath))
            {
                throw std::runtime_error("opened file escapes the configured asset root");
            }

            std::string contents;
            contents.resize(static_cast<std::size_t>(fileStat.st_size));
            std::size_t offset = 0;
            while (offset < contents.size())
            {
                const ssize_t bytesRead = read(file.descriptor, contents.data() + offset, contents.size() - offset);
                if (bytesRead < 0)
                {
                    throw std::runtime_error(std::string("unable to read opened file: ") + std::strerror(errno));
                }
                if (bytesRead == 0)
                {
                    throw std::runtime_error("opened file ended before expected size");
                }

                offset += static_cast<std::size_t>(bytesRead);
            }

            return contents;
#else
            std::error_code sizeError;
            const std::uintmax_t fileSize = std::filesystem::file_size(filePath, sizeError);
            if (sizeError || fileSize > maxFileSizeBytes)
            {
                throw std::runtime_error("unable to inspect file size or file exceeds the configured chart size limit");
            }

            std::ifstream file(filePath, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("unable to open file");
            }

            std::string contents;
            contents.resize(static_cast<std::size_t>(fileSize));
            if (!contents.empty())
            {
                file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
            }
            if (!file && file.gcount() != static_cast<std::streamsize>(contents.size()))
            {
                throw std::runtime_error("unable to read file");
            }

            return contents;
#endif
        }
    }

    BeatPosition BeatPosition::fromTuple(
        std::int64_t whole,
        std::int64_t fractionNumerator,
        std::int64_t fractionDenominator)
    {
        if (whole < 0)
        {
            throw std::runtime_error("beat whole value must be non-negative");
        }
        if (fractionDenominator <= 0)
        {
            throw std::runtime_error("beat denominator must be greater than zero");
        }
        if (fractionNumerator < 0 || fractionNumerator >= fractionDenominator)
        {
            throw std::runtime_error("beat fraction numerator must be in range [0, denominator)");
        }
        if (whole > MAX_BEAT_TUPLE_COMPONENT ||
            fractionNumerator > MAX_BEAT_TUPLE_COMPONENT ||
            fractionDenominator > MAX_BEAT_TUPLE_COMPONENT)
        {
            throw std::runtime_error("beat tuple components exceed the supported range");
        }
        const std::int64_t beatWhole = whole * 4;
        if (beatWhole > (std::numeric_limits<std::int64_t>::max() - (fractionNumerator * 4)) / fractionDenominator)
        {
            throw std::runtime_error("beat position is too large to normalize");
        }

        std::int64_t numerator = beatWhole * fractionDenominator + (fractionNumerator * 4);
        std::int64_t denominator = fractionDenominator;
        const std::int64_t divisor = std::gcd(numerator, denominator);
        numerator /= divisor;
        denominator /= divisor;
        return {numerator, denominator};
    }

    bool operator==(const BeatPosition& lhs, const BeatPosition& rhs) noexcept
    {
        return lhs.numerator == rhs.numerator && lhs.denominator == rhs.denominator;
    }

    bool operator<(const BeatPosition& lhs, const BeatPosition& rhs) noexcept
    {
        return static_cast<long double>(lhs.numerator) / static_cast<long double>(lhs.denominator) <
            static_cast<long double>(rhs.numerator) / static_cast<long double>(rhs.denominator);
    }

    bool Note::isHold() const noexcept
    {
        return endBeat.has_value();
    }

    ChartLoader::ChartLoader(const ChartLoadOptions& options)
        : m_options(options)
    {
        if (m_options.laneCount <= 0)
        {
            throw std::runtime_error("Invalid chart loader options: laneCount must be greater than zero");
        }
        if (m_options.maxFileSizeBytes == 0)
        {
            throw std::runtime_error("Invalid chart loader options: maxFileSizeBytes must be greater than zero");
        }

        m_options.assetRoot = resolveAssetRoot(m_options.assetRoot);
    }

    const std::filesystem::path& ChartLoader::getAssetRoot() const noexcept
    {
        return m_options.assetRoot;
    }

    Chart ChartLoader::loadFromJsonText(std::string_view jsonText) const
    {
        Json root;
        try
        {
            root = Json::parse(jsonText.begin(), jsonText.end());
        }
        catch (const Json::parse_error& exception)
        {
            throw std::runtime_error(std::string("Invalid chart JSON: ") + exception.what());
        }

        if (!root.is_object())
        {
            throw chartError("root must be an object");
        }
        requireField(root, "meta", "root");
        requireField(root, "time", "root");
        requireField(root, "note", "root");

        Chart chart{};
        chart.laneCount = m_options.laneCount;
        chart.meta = parseMetadata(root.at("meta"));
        chart.timingPoints = parseTimingPoints(root.at("time"));
        chart.notes = parseNotes(root.at("note"), m_options.laneCount);
        return chart;
    }

    Chart ChartLoader::loadFromFile(const std::filesystem::path& filePath) const
    {
        std::filesystem::path resolvedPath;
        try
        {
            resolvedPath = resolveChartFilePath(m_options.assetRoot, filePath);
        }
        catch (const std::runtime_error& exception)
        {
            throw std::runtime_error("Invalid chart file '" + filePath.string() + "': " + exception.what());
        }

        std::string contents;
        try
        {
            contents = readRegularFileNoFollow(m_options.assetRoot, resolvedPath, m_options.maxFileSizeBytes);
        }
        catch (const std::runtime_error& exception)
        {
            throw std::runtime_error("Invalid chart file '" + filePath.string() + "': " + exception.what());
        }

        try
        {
            return loadFromJsonText(contents);
        }
        catch (const std::runtime_error& exception)
        {
            throw std::runtime_error("Invalid chart file '" + filePath.string() + "': " + exception.what());
        }
    }

    Chart ChartLoader::loadFromFile(const std::string& filePath) const
    {
        return loadFromFile(std::filesystem::u8path(filePath));
    }
}
