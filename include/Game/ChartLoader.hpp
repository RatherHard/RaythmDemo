// RaythmDemo - Game Chart Loader
// Declares JSON chart parsing and file loading for the Game module.
// Author: RatherHard
// Date: 2026-07-05

#pragma once

#include "Game/Chart.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace Raythm::Game
{
    /** @brief Maximum chart JSON file size accepted by loadFromFile in bytes. */
    inline constexpr std::uintmax_t MAX_CHART_FILE_SIZE_BYTES = 1024U * 1024U;

    /**
     * @brief Options controlling chart parsing and validation.
     */
    struct ChartLoadOptions
    {
        /** @brief Number of lanes accepted for note column validation. */
        int laneCount = static_cast<int>(GAME_LANE_COUNT);

        /** @brief Required root directory that chart files must stay inside when loading from disk. */
        std::filesystem::path assetRoot;

        /** @brief Maximum chart file size accepted by loadFromFile in bytes. */
        std::uintmax_t maxFileSizeBytes = MAX_CHART_FILE_SIZE_BYTES;
    };

    /**
     * @brief Parses documented JSON chart data into runtime Chart values.
     */
    class ChartLoader
    {
    public:
        /**
         * @brief Creates a chart loader with validation options.
         * @param options Lane count and future parser configuration.
         * @throws std::runtime_error when options are invalid.
         * @note Construction performs only cheap option validation.
         */
        explicit ChartLoader(const ChartLoadOptions& options = {});

        /**
         * @brief Returns the canonical asset root used for file loading.
         * @return Root directory that loadFromFile confines chart paths to.
         * @note The default root is the process current working directory at loader construction time.
         */
        [[nodiscard]] const std::filesystem::path& getAssetRoot() const noexcept;

        /**
         * @brief Parses chart JSON text into a runtime Chart.
         * @param jsonText UTF-8 JSON chart content.
         * @return Parsed and validated chart data with sorted timing points and notes.
         * @throws std::runtime_error when JSON syntax or chart validation fails.
         * @note The pure parsing seam still applies ChartLoadOptions::maxFileSizeBytes to bound in-memory inputs.
         */
        [[nodiscard]] Chart loadFromJsonText(std::string_view jsonText) const;

        /**
         * @brief Reads and parses a chart JSON file from disk.
         * @param filePath Path to the chart JSON file, resolved under ChartLoadOptions::assetRoot.
         * @return Parsed and validated chart data.
         * @throws std::runtime_error when reading or parsing fails.
         * @note The file must stay inside the configured asset root and must not exceed the configured size limit.
         */
        [[nodiscard]] Chart loadFromFile(const std::filesystem::path& filePath) const;

        /**
         * @brief Reads and parses a UTF-8 chart JSON file path from disk.
         * @param filePath UTF-8 path to the chart JSON file, resolved under ChartLoadOptions::assetRoot.
         * @return Parsed and validated chart data.
         * @throws std::runtime_error when reading or parsing fails.
         * @note Prefer the filesystem::path overload when the caller already has a native platform path.
         */
        [[nodiscard]] Chart loadFromFile(const std::string& filePath) const;

    private:
        /** @brief Validation options retained by the loader. */
        ChartLoadOptions m_options{};
    };
}
