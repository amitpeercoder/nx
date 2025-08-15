#include "nx/import_export/exporter.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <ctime>

#include "nx/util/safe_process.hpp"

namespace nx::import_export {

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
    {ExportFormat::Pdf, "PDF files (requires external tools)"}
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

    std::ofstream file(note_path);
    if (!file) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                       "Failed to create HTML file: " + note_path.string()));
    }

    file << full_page;
    if (!file) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                       "Failed to write HTML file: " + note_path.string()));
    }
  }

  // Create index page
  std::string index_html = generateIndexPage(notes);
  std::filesystem::path index_path = options.output_path / "index.html";
  
  std::ofstream index_file(index_path);
  if (index_file) {
    index_file << generateHtmlPage("Notes Index", index_html);
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
    char* end = strptime(date_str.c_str(), "%Y-%m-%d", &tm);
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
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
      "PDF generation requires either 'pandoc' or 'wkhtmltopdf' to be installed. "
      "Please install one of these tools and ensure it's in your PATH."));
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
      // Create markdown file first
      std::filesystem::path md_path = temp_dir / (note.id().toString() + ".md");
      
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
      
      result = convertWithPandoc(md_path, pdf_path);
    } else if (pdf_tool == "wkhtmltopdf") {
      // Create HTML file first
      HtmlExporter html_exporter;
      std::string html_content = html_exporter.markdownToHtml(note.content());
      std::string full_page = html_exporter.generateHtmlPage(note.title(), html_content, options.template_file);
      
      std::filesystem::path html_path = temp_dir / (note.id().toString() + ".html");
      std::ofstream html_file(html_path);
      if (!html_file) {
        std::filesystem::remove_all(temp_dir, ec);
        return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                         "Failed to create HTML file: " + html_path.string()));
      }
      html_file << full_page;
      html_file.close();
      
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
  // Check for pandoc first (preferred)
  if (nx::util::SafeProcess::commandExists("pandoc")) {
    return "pandoc";
  }
  
  // Check for wkhtmltopdf as fallback
  if (nx::util::SafeProcess::commandExists("wkhtmltopdf")) {
    return "wkhtmltopdf";
  }
  
  return "";
}

Result<void> PdfExporter::convertWithPandoc(const std::filesystem::path& markdown_path,
                                           const std::filesystem::path& pdf_path) const {
  // Use pandoc to convert markdown to PDF
  auto result = nx::util::SafeProcess::execute("pandoc", 
    {markdown_path.string(), "-o", pdf_path.string(), "--pdf-engine=xelatex"});
  
  if (!result.has_value() || !result->success()) {
    // Try without specific PDF engine
    result = nx::util::SafeProcess::execute("pandoc", 
      {markdown_path.string(), "-o", pdf_path.string()});
    
    if (!result.has_value() || !result->success()) {
      return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                       "Failed to convert to PDF with pandoc: " + 
                                       (result.has_value() ? result->stderr_output : "unknown error")));
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

} // namespace nx::import_export