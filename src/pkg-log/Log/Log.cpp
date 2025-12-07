#include "Log.h"
#include <spdlog/pattern_formatter.h>
#include <spdlog/details/os.h>

namespace Log
{
    Logger Logger::Default;
}

namespace Log::Detail
{
    /// Introduce %N custom logger area name formatter or short filename w/ line number.
    class LoggerFlagFormatter final : public spdlog::custom_flag_formatter
    {
    public:
        static constexpr auto Flag = 'N';

        void format(const spdlog::details::log_msg& msg, const std::tm& time, spdlog::memory_buf_t& dest) override
        {
            const auto& source = msg.source;
            if (source.line == AreaLoggerLine) {
                dest.append(source.filename); // filename contains area name
            } else {
                // Skip append basename
                // shortFilenameFormatter.format(msg, time, dest);

                const auto basename = FileBasename(source.filename);
                const auto area = RemoveExtension(basename);
                spdlog::details::fmt_helper::append_string_view(area, dest);

                // Skip line number output
                // if (!source.empty()) {
                //     dest.append(":");
                //     spdlog::details::fmt_helper::append_int(msg.source.line, dest);
                // }
            }
        }

        [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override
        {
            return spdlog::details::make_unique<LoggerFlagFormatter>();
        }

    private:
        using ShortFilenameFormatter = spdlog::details::short_filename_formatter<spdlog::details::null_scoped_padder>;
        ShortFilenameFormatter shortFilenameFormatter{spdlog::details::padding_info{}};

        static std::string_view FileBasename(const char* filename)
        {
            using namespace spdlog::details;

            const auto* end = filename + std::strlen(filename);

            // if the size is 2 (1 character + null terminator) we can use the more efficient strrchr the branch will be elided by optimizations
            if constexpr (sizeof(os::folder_seps) == 2) {
                constexpr auto sep = os::folder_seps[0];
                // const char* rv = std::strrchr(filename, sep);
                // return rv != nullptr ? rv + 1 : filename;
                const auto* p = end + 1;
                while (p-- > filename) {
                    if (*p == sep) {
                        return {p + 1, static_cast<size_t>(end - p)};
                    }
                }
                // ReSharper disable once CppDFALocalValueEscapesFunction
                return {filename, static_cast<size_t>(end - filename)};
            }

            const auto* p = end;

            // go back until separator
            while (p-- > filename) {
                const auto c = *p;
                for (const auto sep : os::folder_seps) {
                    if (c == sep) {
                        return {p + 1, static_cast<size_t>(end - p)};
                    }
                }
            }

            // ReSharper disable once CppDFALocalValueEscapesFunction
            return {filename, static_cast<size_t>(end - filename)};
        }

        static std::string_view RemoveExtension(std::string_view name)
        {
            const auto* start = name.data();
            const auto* p = start + name.size();
            while (p > start) {
                --p;
                if (*p == '.') {
                    // ReSharper disable once CppDFALocalValueEscapesFunction
                    return {start, static_cast<size_t>(p - start)};
                }
            }
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return name;
        }
    };

    /// Optional function name formatter (%!) w/ suffix only if function name is present.
    /// Avoids printing empty function names and suffix to prettify output.
    /// Based on spdlog::details::source_funcname_formatter<spdlog::details::null_scoped_padder>.
    class FunctionNameFlagFormatter final : public spdlog::custom_flag_formatter
    {
    public:
        static constexpr auto Flag = '&';

        void format(const spdlog::details::log_msg& msg, const std::tm& time, spdlog::memory_buf_t& dest) override
        {
            const auto* funcname = msg.source.funcname;
            if (funcname) {
                const auto shortName = ExtractShortNameWithNamespace(funcname);
                spdlog::details::fmt_helper::append_string_view(shortName, dest);
                dest.append(": ");
            }
        }

        [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override
        {
            return spdlog::details::make_unique<FunctionNameFlagFormatter>();
        }

    private:
        constexpr static std::string_view ExtractShortNameWithNamespace(const char* full_name) {
            // Find the first open brace - end of the name
            const auto* func_end = full_name;
            while (*func_end && *func_end != '(') {
                ++func_end;
            }

            // Find back first space or ':' — start of the name
            const auto* func_start = func_end;
            while (func_start > full_name) {
                const auto c = *(func_start - 1);
                if (c == ' ' || c == ':') {
                    break;
                }
                --func_start;
            }

            // ReSharper disable once CppDFALocalValueEscapesFunction
            return {func_start, static_cast<size_t>(func_end - func_start)};
        }
    };

    void DefaultInit()
    {
        spdlog::set_level(spdlog::level::trace);

        // https://github.com/gabime/spdlog/wiki/Custom-formatting
        // spdlog::set_pattern("(%T.%f) %t %^[%L]%$ [%s:%#] %!: %v");
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<LoggerFlagFormatter>(LoggerFlagFormatter::Flag);
        formatter->add_flag<FunctionNameFlagFormatter>(FunctionNameFlagFormatter::Flag);

#if defined(__EMSCRIPTEN__)
        // emscripten timer accuracy is limited to milliseconds
    #define SEC_FRAC_FORMAT "%e" // NOLINT(cppcoreguidelines-macro-usage)
#else
    #define SEC_FRAC_FORMAT "%f" // NOLINT(cppcoreguidelines-macro-usage)
#endif
        formatter->set_pattern("(%T." SEC_FRAC_FORMAT ") %t %^[%L]%$ [%N] %&%v");
        spdlog::set_formatter(std::move(formatter));
    }
}
