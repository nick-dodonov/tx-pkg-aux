#include "Log.h"

#include <Log/Path.h>
#include <spdlog/pattern_formatter.h>

namespace Log
{
    Logger Logger::Default;
}

namespace Log::Detail
{
    /// Introduce %N custom logger area name formatter or short filename w/ line number.
    /// Based on spdlog::details::short_filename_formatter
    class LoggerFlagFormatter final : public spdlog::custom_flag_formatter
    {
    public:
        static constexpr auto Flag = 'N';

        void format(const spdlog::details::log_msg& msg, const std::tm& time, spdlog::memory_buf_t& dest) override
        {
            const auto& source = msg.source;
            if (source.line == AreaLoggerLine) {
                spdlog::details::scoped_padder p{padinfo_.enabled() ? std::strlen(source.filename): 0, padinfo_, dest};
                dest.append(source.filename); // filename contains area name
            } else {
                // Skip append basename
                // shortFilenameFormatter.format(msg, time, dest);

                const auto basename = Path::GetBasename(source.filename);
                const auto area = Path::GetWithoutExtension(basename);

                spdlog::details::scoped_padder p{padinfo_.enabled() ? area.size(): 0, padinfo_, dest};
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
        // ShortFilenameFormatter shortFilenameFormatter{spdlog::details::padding_info{}};
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
        formatter->set_pattern("(%T." SEC_FRAC_FORMAT ") %t %^[%L]%$ %12!N: %&%v");
        spdlog::set_formatter(std::move(formatter));
    }
}
