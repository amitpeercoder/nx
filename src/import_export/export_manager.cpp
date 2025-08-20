#include "nx/import_export/exporter.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <iostream>

#include "nx/util/safe_process.hpp"
#include "nx/util/filesystem.hpp"

namespace nx::import_export {

// Helper function to detect platform for better error messages
std::string getPlatformSpecificInstructions(const std::string& tool) {
#ifdef _WIN32
  if (tool == "pandoc") {
    return "Windows: winget install pandoc; winget install MiKTeX.MiKTeX";
  } else if (tool == "weasyprint") {
    return "Windows: pip3 install weasyprint";
  } else if (tool == "wkhtmltopdf") {
    return "Windows: Download from wkhtmltopdf.org";
  }
#elif __APPLE__
  if (tool == "pandoc") {
    return "macOS: brew install pandoc basictex";
  } else if (tool == "weasyprint") {
    return "macOS: pip3 install weasyprint";
  } else if (tool == "wkhtmltopdf") {
    return "macOS: brew install wkhtmltopdf";
  }
#else // Linux
  if (tool == "pandoc") {
    return "Linux: apt install pandoc texlive-latex-base (Ubuntu/Debian) or yum install pandoc texlive-latex (CentOS/RHEL)";
  } else if (tool == "weasyprint") {
    return "Linux: pip3 install weasyprint (may need: apt install libpango-1.0-0 libharfbuzz0b libpangoft2-1.0-0)";
  } else if (tool == "wkhtmltopdf") {
    return "Linux: apt install wkhtmltopdf (Ubuntu/Debian) or yum install wkhtmltopdf (CentOS/RHEL)";
  }
#endif
  return "";
}

Result<std::unique_ptr<Exporter>> ExportManager::createExporter(ExportFormat format) {
  switch (format) {
    case ExportFormat::Markdown:
      return std::make_unique<MarkdownExporter>();
    
    case ExportFormat::Json:
      return std::make_unique<JsonExporter>();
    
    case ExportFormat::Zip: {
      // Default to markdown for ZIP base exporter
      auto base_exporter = std::make_unique<MarkdownExporter>();
      return std::make_unique<ZipExporter>(std::move(base_exporter));
    }
    
    case ExportFormat::Html:
      return std::make_unique<HtmlExporter>();
    
    case ExportFormat::Pdf:
      return std::make_unique<PdfExporter>();
    
    default:
      return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                       "Unknown export format"));
  }
}

Result<void> ExportManager::exportNotes(const std::vector<nx::core::Note>& notes,
                                        const ExportOptions& options) {
  if (notes.empty()) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "No notes to export"));
  }

  // Filter notes based on options
  auto filtered_notes = filterNotes(notes, options);
  if (filtered_notes.empty()) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "No notes match the export criteria"));
  }

  // Create exporter
  auto exporter_result = createExporter(options.format);
  if (!exporter_result.has_value()) {
    return std::unexpected(exporter_result.error());
  }

  auto& exporter = exporter_result.value();

  // Perform export
  return exporter->exportNotes(filtered_notes, options);
}

std::vector<nx::core::Note> ExportManager::filterNotes(const std::vector<nx::core::Note>& notes,
                                                       const ExportOptions& options) {
  std::vector<nx::core::Note> filtered;
  
  for (const auto& note : notes) {
    bool include = true;
    
    // Tag filter
    if (!options.tag_filter.empty()) {
      const auto& note_tags = note.metadata().tags();
      bool has_matching_tag = false;
      for (const auto& filter_tag : options.tag_filter) {
        if (std::find(note_tags.begin(), note_tags.end(), filter_tag) != note_tags.end()) {
          has_matching_tag = true;
          break;
        }
      }
      if (!has_matching_tag) {
        include = false;
      }
    }
    
    // Notebook filter
    if (include && options.notebook_filter.has_value()) {
      if (!note.notebook().has_value() || 
          note.notebook().value() != options.notebook_filter.value()) {
        include = false;
      }
    }
    
    // Date filter implementation
    if (include && options.date_filter.has_value()) {
      auto date_result = parseDateFilter(options.date_filter.value());
      if (date_result.has_value()) {
        auto [start_date, end_date] = *date_result;
        auto note_date = note.metadata().updated(); // Use updated date for filtering
        
        if (start_date.has_value() && note_date < *start_date) {
          include = false;
        }
        if (include && end_date.has_value() && note_date > *end_date) {
          include = false;
        }
      }
      // If date parsing fails, include the note (graceful degradation)
    }
    
    if (include) {
      filtered.push_back(note);
    }
  }
  
  return filtered;
}

std::map<ExportFormat, std::string> ExportManager::getSupportedFormats() {
  return {
    {ExportFormat::Markdown, "Markdown files with YAML front-matter"},
    {ExportFormat::Json, "JSON format with full metadata"},
    {ExportFormat::Zip, "ZIP archive containing exported files"},
    {ExportFormat::Html, "HTML files with styling"},
    {ExportFormat::Pdf, "PDF files (requires pandoc+LaTeX, weasyprint, or wkhtmltopdf)"}
  };
}

Result<ExportFormat> ExportManager::parseFormat(const std::string& format_string) {
  std::string lower_format = format_string;
  std::transform(lower_format.begin(), lower_format.end(), lower_format.begin(), ::tolower);
  
  if (lower_format == "markdown" || lower_format == "md") {
    return ExportFormat::Markdown;
  } else if (lower_format == "json") {
    return ExportFormat::Json;
  } else if (lower_format == "zip") {
    return ExportFormat::Zip;
  } else if (lower_format == "html" || lower_format == "htm") {
    return ExportFormat::Html;
  } else if (lower_format == "pdf") {
    return ExportFormat::Pdf;
  } else {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "Unknown export format: " + format_string));
  }
}

// ZipExporter implementation

ZipExporter::ZipExporter(std::unique_ptr<Exporter> base_exporter)
  : base_exporter_(std::move(base_exporter)) {}

Result<void> ZipExporter::exportNotes(const std::vector<nx::core::Note>& notes, 
                                      const ExportOptions& options) {
  // Create temporary directory for base export
  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / 
                                  ("nx_export_" + std::to_string(std::time(nullptr)));
  
  std::error_code ec;
  std::filesystem::create_directories(temp_dir, ec);
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                     "Failed to create temporary directory: " + ec.message()));
  }

  // Export to temporary directory using base exporter
  ExportOptions temp_options = options;
  temp_options.output_path = temp_dir;
  
  auto export_result = base_exporter_->exportNotes(notes, temp_options);
  if (!export_result.has_value()) {
    // Clean up temporary directory
    std::filesystem::remove_all(temp_dir, ec);
    return std::unexpected(export_result.error());
  }

  // Create ZIP archive
  std::filesystem::path zip_path = options.output_path;
  if (std::filesystem::is_directory(zip_path)) {
    zip_path = zip_path / "notes_export.zip";
  }

  auto zip_result = createZipArchive(temp_dir, zip_path);
  
  // Clean up temporary directory
  std::filesystem::remove_all(temp_dir, ec);
  
  return zip_result;
}

Result<void> ZipExporter::createZipArchive(const std::filesystem::path& source_dir,
                                          const std::filesystem::path& zip_path) const {
  // Check if zip command is available
  if (!nx::util::SafeProcess::commandExists("zip")) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "zip command not found. Please install zip utility."));
  }

  // Ensure output directory exists
  auto parent_dir = zip_path.parent_path();
  if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(parent_dir, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create output directory: " + ec.message()));
    }
  }

  // Create ZIP archive using zip command
  auto zip_result = nx::util::SafeProcess::execute("zip", 
    {"-r", zip_path.string(), "."}, source_dir.string());
  
  if (!zip_result.has_value() || !zip_result->success()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Failed to create ZIP archive: " + 
                                     (zip_result.has_value() ? zip_result->stderr_output : "unknown error")));
  }

  return {};
}

// HtmlExporter implementation

Result<void> HtmlExporter::exportNotes(const std::vector<nx::core::Note>& notes, 
                                       const ExportOptions& options) {
  if (notes.empty()) {
    return {};
  }

  // Create output directory
  if (!std::filesystem::exists(options.output_path)) {
    std::error_code ec;
    std::filesystem::create_directories(options.output_path, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create output directory: " + ec.message()));
    }
  }

  // Export each note as HTML
  for (const auto& note : notes) {
    std::string filename = note.id().toString() + ".html";
    std::filesystem::path note_path = options.output_path / filename;

    std::string html_content = markdownToHtml(note.content());
    std::string full_page = generateHtmlPage(note.title(), html_content, options.template_file);

    // Write HTML file atomically
    auto write_result = nx::util::FileSystem::writeFileAtomic(note_path, full_page);
    if (!write_result.has_value()) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                       "Failed to write HTML file: " + write_result.error().message()));
    }
  }

  // Create index page
  std::string index_html = generateIndexPage(notes);
  std::filesystem::path index_path = options.output_path / "index.html";
  
  // Write index file atomically
  auto index_result = nx::util::FileSystem::writeFileAtomic(index_path, generateHtmlPage("Notes Index", index_html));
  if (!index_result.has_value()) {
    // Don't fail the entire export if index creation fails
    std::cerr << "Warning: Failed to create HTML index file: " << index_result.error().message() << std::endl;
  }

  return {};
}

std::string HtmlExporter::markdownToHtml(const std::string& markdown) const {
  // Enhanced markdown to HTML conversion
  // This implementation covers common markdown features
  std::string html = markdown;
  
  // Convert headers (H1-H6)
  std::regex h6_regex(R"(^###### (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, h6_regex, "<h6>$1</h6>");
  
  std::regex h5_regex(R"(^##### (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, h5_regex, "<h5>$1</h5>");
  
  std::regex h4_regex(R"(^#### (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, h4_regex, "<h4>$1</h4>");
  
  std::regex h3_regex(R"(^### (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, h3_regex, "<h3>$1</h3>");
  
  std::regex h2_regex(R"(^## (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, h2_regex, "<h2>$1</h2>");
  
  std::regex h1_regex(R"(^# (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, h1_regex, "<h1>$1</h1>");
  
  // Convert code blocks (```code```)
  std::regex code_block_regex(R"(```([^`]*?)```)");
  html = std::regex_replace(html, code_block_regex, "<pre><code>$1</code></pre>");
  
  // Convert inline code (`code`)
  std::regex inline_code_regex(R"(`([^`]+)`)");
  html = std::regex_replace(html, inline_code_regex, "<code>$1</code>");
  
  // Convert bold (**bold**)
  std::regex bold_regex(R"(\*\*([^*]+)\*\*)");
  html = std::regex_replace(html, bold_regex, "<strong>$1</strong>");
  
  // Convert italic (*italic*)
  std::regex italic_regex(R"(\*([^*]+)\*)");
  html = std::regex_replace(html, italic_regex, "<em>$1</em>");
  
  // Convert links [text](url)
  std::regex link_regex(R"(\[([^\]]+)\]\(([^)]+)\))");
  html = std::regex_replace(html, link_regex, "<a href=\"$2\">$1</a>");
  
  // Convert unordered lists (- item or * item)
  std::regex unordered_list_regex(R"(^[*-] (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, unordered_list_regex, "<li>$1</li>");
  
  // Convert ordered lists (1. item)
  std::regex ordered_list_regex(R"(^(\d+)\. (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, ordered_list_regex, "<li>$2</li>");
  
  // Wrap consecutive list items in <ul> or <ol> tags
  // This is a simplified approach - a proper parser would handle nesting
  std::regex consecutive_li_regex(R"((<li>.*</li>\s*)+)");
  html = std::regex_replace(html, consecutive_li_regex, "<ul>$&</ul>");
  
  // Convert horizontal rules (--- or ***)
  std::regex hr_regex(R"(^(---|\*\*\*)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, hr_regex, "<hr>");
  
  // Convert blockquotes (> text)
  std::regex blockquote_regex(R"(^> (.+)$)", std::regex_constants::multiline);
  html = std::regex_replace(html, blockquote_regex, "<blockquote>$1</blockquote>");
  
  // Convert paragraph breaks (double newlines)
  std::regex paragraph_regex(R"(\n\n+)");
  html = std::regex_replace(html, paragraph_regex, "</p>\n<p>");
  
  // Wrap in paragraph tags
  html = "<p>" + html + "</p>";
  
  // Clean up empty paragraphs
  std::regex empty_p_regex(R"(<p>\s*</p>)");
  html = std::regex_replace(html, empty_p_regex, "");
  
  // Convert remaining single line breaks to <br>
  std::regex single_newline_regex(R"(\n)");
  html = std::regex_replace(html, single_newline_regex, "<br>\n");
  
  return html;
}

std::string HtmlExporter::generateHtmlPage(const std::string& title, 
                                          const std::string& content,
                                          const std::string& template_file) const {
  // Use custom template if provided
  if (!template_file.empty() && std::filesystem::exists(template_file)) {
    std::ifstream template_stream(template_file);
    if (template_stream) {
      std::string template_content((std::istreambuf_iterator<char>(template_stream)),
                                  std::istreambuf_iterator<char>());
      
      // Simple template substitution
      std::regex title_regex(R"(\{\{title\}\})");
      template_content = std::regex_replace(template_content, title_regex, title);
      
      std::regex content_regex(R"(\{\{content\}\})");
      template_content = std::regex_replace(template_content, content_regex, content);
      
      return template_content;
    }
  }
  
  // Default template
  std::ostringstream page;
  page << "<!DOCTYPE html>\n";
  page << "<html lang=\"en\">\n";
  page << "<head>\n";
  page << "  <meta charset=\"UTF-8\">\n";
  page << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
  page << "  <title>" << title << "</title>\n";
  page << "  <style>\n";
  page << "    body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }\n";
  page << "    h1, h2, h3 { color: #333; }\n";
  page << "    .content { line-height: 1.6; }\n";
  page << "  </style>\n";
  page << "</head>\n";
  page << "<body>\n";
  page << "  <h1>" << title << "</h1>\n";
  page << "  <div class=\"content\">\n";
  page << content << "\n";
  page << "  </div>\n";
  page << "</body>\n";
  page << "</html>\n";
  
  return page.str();
}

std::string HtmlExporter::generateIndexPage(const std::vector<nx::core::Note>& notes) const {
  std::ostringstream index;
  index << "<h2>Exported Notes</h2>\n";
  index << "<p>Total notes: " << notes.size() << "</p>\n";
  index << "<ul>\n";
  
  for (const auto& note : notes) {
    index << "  <li><a href=\"" << note.id().toString() << ".html\">" 
          << note.title() << "</a></li>\n";
  }
  
  index << "</ul>\n";
  return index.str();
}

// Date filter parsing implementation
std::optional<std::pair<std::optional<std::chrono::system_clock::time_point>, 
                       std::optional<std::chrono::system_clock::time_point>>> 
ExportManager::parseDateFilter(const std::string& date_filter) {
  // Support formats:
  // "2024-01-01" (single date - notes from that date)
  // "2024-01-01:2024-12-31" (date range)
  // "after:2024-01-01" (after date)
  // "before:2024-12-31" (before date)
  
  std::optional<std::chrono::system_clock::time_point> start_date;
  std::optional<std::chrono::system_clock::time_point> end_date;
  
  if (date_filter.starts_with("after:")) {
    auto date_str = date_filter.substr(6);
    start_date = parseIsoDate(date_str);
  } else if (date_filter.starts_with("before:")) {
    auto date_str = date_filter.substr(7);
    end_date = parseIsoDate(date_str);
  } else if (date_filter.find(':') != std::string::npos) {
    // Range format "start:end"
    auto colon_pos = date_filter.find(':');
    auto start_str = date_filter.substr(0, colon_pos);
    auto end_str = date_filter.substr(colon_pos + 1);
    
    start_date = parseIsoDate(start_str);
    end_date = parseIsoDate(end_str);
  } else {
    // Single date - treat as "on this date" (start of day to end of day)
    auto date = parseIsoDate(date_filter);
    if (date.has_value()) {
      start_date = date;
      // Add 24 hours for end of day
      end_date = *date + std::chrono::hours(24);
    }
  }
  
  if (start_date.has_value() || end_date.has_value()) {
    return std::make_pair(start_date, end_date);
  }
  
  return std::nullopt; // Parsing failed
}

std::optional<std::chrono::system_clock::time_point> 
ExportManager::parseIsoDate(const std::string& date_str) {
  // Parse ISO date format: YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS
  std::tm tm = {};
  
  // Try parsing YYYY-MM-DD format first
  if (date_str.length() >= 10) {
#ifdef _WIN32
    // Windows doesn't have strptime, use alternative parsing
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
      return std::nullopt;
    }
    char* end = const_cast<char*>(date_str.c_str() + 10); // Assuming 10 char date
#else
    char* end = strptime(date_str.c_str(), "%Y-%m-%d", &tm);
#endif
    if (end != nullptr) {
      // Successfully parsed date part
      auto time_t = std::mktime(&tm);
      if (time_t != -1) {
        return std::chrono::system_clock::from_time_t(time_t);
      }
    }
  }
  
  return std::nullopt;
}

// PdfExporter implementation

Result<void> PdfExporter::exportNotes(const std::vector<nx::core::Note>& notes, 
                                      const ExportOptions& options) {
  if (notes.empty()) {
    return {};
  }

  // Check if PDF tools are available
  std::string pdf_tool = findPdfTool();
  if (pdf_tool.empty()) {
    std::string platform_specific = getPlatformSpecificInstructions("weasyprint");
    std::string error_msg = 
      "PDF generation requires either 'pandoc', 'weasyprint', or 'wkhtmltopdf' to be installed.\n"
      "Please install one of these tools and ensure it's in your PATH.\n\n";
    
    if (!platform_specific.empty()) {
      error_msg += "Quick install for your platform:\n" + platform_specific + "\n\n";
    }
    
    error_msg += 
      "All platform installation options:\n"
      "1. Weasyprint (Python-based, easiest):\n"
      "   pip3 install weasyprint\n"
      "   (May require system dependencies on Linux)\n\n"
      "2. Pandoc + LaTeX (best markdown support):\n"
      "   • Ubuntu/Debian: apt install pandoc texlive-latex-base\n"
      "   • CentOS/RHEL: yum install pandoc texlive-latex\n"
      "   • macOS: brew install pandoc basictex\n"
      "   • Windows: winget install pandoc; winget install MiKTeX.MiKTeX\n\n"
      "3. wkhtmltopdf (HTML to PDF):\n"
      "   • Ubuntu/Debian: apt install wkhtmltopdf\n"
      "   • CentOS/RHEL: yum install wkhtmltopdf\n"
      "   • macOS: brew install wkhtmltopdf\n"
      "   • Windows: Download from wkhtmltopdf.org";
    
    return std::unexpected(makeError(ErrorCode::kExternalToolError, error_msg));
  }

  // Create output directory
  if (!std::filesystem::exists(options.output_path)) {
    std::error_code ec;
    std::filesystem::create_directories(options.output_path, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create output directory: " + ec.message()));
    }
  }

  // Create temporary directory for intermediate files
  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / 
                                  ("nx_pdf_export_" + std::to_string(std::time(nullptr)));
  
  std::error_code ec;
  std::filesystem::create_directories(temp_dir, ec);
  if (ec) {
    return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                     "Failed to create temporary directory: " + ec.message()));
  }

  // Export each note as PDF
  for (const auto& note : notes) {
    std::string filename = note.id().toString() + ".pdf";
    std::filesystem::path pdf_path = options.output_path / filename;

    Result<void> result;
    
    if (pdf_tool == "pandoc") {
      // Generate markdown content with metadata
      MarkdownExporter md_exporter;
      ExportOptions md_options = options;
      md_options.output_path = temp_dir;
      md_options.format = ExportFormat::Markdown;
      
      std::vector<nx::core::Note> single_note = {note};
      auto md_result = md_exporter.exportNotes(single_note, md_options);
      if (!md_result.has_value()) {
        std::filesystem::remove_all(temp_dir, ec);
        return std::unexpected(md_result.error());
      }
      
      // Find the actual created markdown file (uses title-based naming)
      std::string expected_filename = md_exporter.generateFilename(note, ".md");
      std::filesystem::path md_path = temp_dir / expected_filename;
      
      // Verify the file exists before calling pandoc
      if (!std::filesystem::exists(md_path)) {
        std::filesystem::remove_all(temp_dir, ec);
        return std::unexpected(makeError(ErrorCode::kFileError,
                                        "Markdown file not found: " + md_path.string()));
      }
      
      result = convertWithPandoc(md_path, pdf_path);
    } else if (pdf_tool == "weasyprint") {
      // Create HTML file first and convert with weasyprint
      HtmlExporter html_exporter;
      std::string html_content = html_exporter.markdownToHtml(note.content());
      std::string full_page = html_exporter.generateHtmlPage(note.title(), html_content, options.template_file);
      
      std::filesystem::path html_path = temp_dir / (note.id().toString() + ".html");
      // Write temporary HTML file atomically
      auto html_result = nx::util::FileSystem::writeFileAtomic(html_path, full_page);
      if (!html_result.has_value()) {
        std::filesystem::remove_all(temp_dir, ec);
        return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                         "Failed to create HTML file: " + html_result.error().message()));
      }
      
      result = convertWithWeasyprint(html_path, pdf_path);
    } else if (pdf_tool == "wkhtmltopdf") {
      // Create HTML file first
      HtmlExporter html_exporter;
      std::string html_content = html_exporter.markdownToHtml(note.content());
      std::string full_page = html_exporter.generateHtmlPage(note.title(), html_content, options.template_file);
      
      std::filesystem::path html_path = temp_dir / (note.id().toString() + ".html");
      // Write temporary HTML file atomically
      auto html_result = nx::util::FileSystem::writeFileAtomic(html_path, full_page);
      if (!html_result.has_value()) {
        std::filesystem::remove_all(temp_dir, ec);
        return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                         "Failed to create HTML file: " + html_result.error().message()));
      }
      
      result = convertWithWkhtmltopdf(html_path, pdf_path);
    } else {
      result = std::unexpected(makeError(ErrorCode::kExternalToolError,
                                        "Unknown PDF tool: " + pdf_tool));
    }

    if (!result.has_value()) {
      std::filesystem::remove_all(temp_dir, ec);
      return std::unexpected(result.error());
    }
  }

  // Clean up temporary directory
  std::filesystem::remove_all(temp_dir, ec);
  
  return {};
}

std::string PdfExporter::findPdfTool() const {
  // Check for pandoc first (preferred for markdown) - but verify it can actually work
  if (nx::util::SafeProcess::commandExists("pandoc")) {
    // Test if pandoc has a working PDF engine by checking common ones across platforms
    if (nx::util::SafeProcess::commandExists("pdflatex") || 
        nx::util::SafeProcess::commandExists("xelatex") ||
        nx::util::SafeProcess::commandExists("lualatex") ||
        nx::util::SafeProcess::commandExists("latex") ||
        nx::util::SafeProcess::commandExists("miktex") ||
        nx::util::SafeProcess::commandExists("texlive")) {
      return "pandoc";
    }
  }
  
  // Check for weasyprint (Python-based HTML to PDF, cross-platform)
  if (nx::util::SafeProcess::commandExists("weasyprint")) {
    return "weasyprint";
  }
  
  // Check for wkhtmltopdf (available on most platforms)
  if (nx::util::SafeProcess::commandExists("wkhtmltopdf")) {
    return "wkhtmltopdf";
  }
  
  // Additional Windows-specific checks
  if (nx::util::SafeProcess::commandExists("wkhtmltopdf.exe")) {
    return "wkhtmltopdf";
  }
  
  // If pandoc exists but no PDF engines detected, still return it as a last resort
  // The user might have a LaTeX distribution we didn't detect
  if (nx::util::SafeProcess::commandExists("pandoc")) {
    return "pandoc";
  }
  
  return "";
}

Result<void> PdfExporter::convertWithPandoc(const std::filesystem::path& markdown_path,
                                           const std::filesystem::path& pdf_path) const {
  // Try without specific PDF engine first (let pandoc choose the default)
  auto result = nx::util::SafeProcess::execute("pandoc", 
    {markdown_path.string(), "-o", pdf_path.string()});
  
  if (!result.has_value() || !result->success()) {
    // Try with xelatex if the default failed
    result = nx::util::SafeProcess::execute("pandoc", 
      {markdown_path.string(), "-o", pdf_path.string(), "--pdf-engine=xelatex"});
    
    if (!result.has_value() || !result->success()) {
      return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                       "Failed to convert to PDF with pandoc: " + 
                                       (result.has_value() ? result->stderr_output : "unknown error") +
                                       "\n\nTo fix this, install a LaTeX distribution:\n"
                                       "• Ubuntu/Debian: apt install texlive-latex-base\n"
                                       "• CentOS/RHEL: yum install texlive-latex\n"
                                       "• macOS: brew install basictex\n"
                                       "• Windows: winget install MiKTeX.MiKTeX or choco install miktex"));
    }
  }

  return {};
}

Result<void> PdfExporter::convertWithWkhtmltopdf(const std::filesystem::path& html_path,
                                                const std::filesystem::path& pdf_path) const {
  // Use wkhtmltopdf to convert HTML to PDF
  auto result = nx::util::SafeProcess::execute("wkhtmltopdf", 
    {"--page-size", "A4", "--margin-top", "0.75in", "--margin-bottom", "0.75in",
     html_path.string(), pdf_path.string()});
  
  if (!result.has_value() || !result->success()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Failed to convert to PDF with wkhtmltopdf: " + 
                                     (result.has_value() ? result->stderr_output : "unknown error")));
  }

  return {};
}

Result<void> PdfExporter::convertWithWeasyprint(const std::filesystem::path& html_path,
                                               const std::filesystem::path& pdf_path) const {
  // Use weasyprint to convert HTML to PDF
  auto result = nx::util::SafeProcess::execute("weasyprint", 
    {html_path.string(), pdf_path.string()});
  
  if (!result.has_value() || !result->success()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Failed to convert to PDF with weasyprint: " + 
                                     (result.has_value() ? result->stderr_output : "unknown error")));
  }

  return {};
}

} // namespace nx::import_export