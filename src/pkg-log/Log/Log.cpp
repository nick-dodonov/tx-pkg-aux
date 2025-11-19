#include "Log.h"
#include <spdlog/pattern_formatter.h>

namespace Log
{
    Logger Logger::Default;
}

namespace Log::Details
{
    /// Introduce %N custom logger area name formatter or short filename w/ line number.
    class LoggerFlagFormatter : public spdlog::custom_flag_formatter
    {
    public:
        static constexpr char Flag = 'N';

        void format(const spdlog::details::log_msg& msg, const std::tm& time, spdlog::memory_buf_t& dest) override
        {
            const auto& source = msg.source;
            if (source.line == Details::AreaLoggerLine) {
                dest.append(source.filename); // filename contains area name
            } else {
                shortFilenameFormatter.format(msg, time, dest);
                if (!source.empty()) {
                    dest.append(":");
                    spdlog::details::fmt_helper::append_int(msg.source.line, dest);
                }
            }
        }

        [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override
        {
            return spdlog::details::make_unique<LoggerFlagFormatter>();
        }

    private:
        using ShortFilenameFormatter = spdlog::details::short_filename_formatter<spdlog::details::null_scoped_padder>;
        ShortFilenameFormatter shortFilenameFormatter{spdlog::details::padding_info{}};
    };

    /// Optional function name formatter (%!) w/ suffix only if function name is present.
    /// Avoids printing empty function names and suffix to prettify output.
    /// Based on spdlog::details::source_funcname_formatter<spdlog::details::null_scoped_padder>.
    class FunctionNameFlagFormatter : public spdlog::custom_flag_formatter
    {
    public:
        static constexpr char Flag = '&';

        void format(const spdlog::details::log_msg& msg, const std::tm& time, spdlog::memory_buf_t& dest) override
        {
            const auto* funcname = msg.source.funcname;
            if (funcname) {
                spdlog::details::fmt_helper::append_string_view(funcname, dest);
                dest.append(": ");
            }
        }

        [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override
        {
            return spdlog::details::make_unique<FunctionNameFlagFormatter>();
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
