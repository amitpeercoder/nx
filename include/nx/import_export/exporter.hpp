#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <memory>
#include <map>
#include <chrono>

#include <nlohmann/json.hpp>

#include "nx/common.hpp"
#include "nx/core/note.hpp"
#include "nx/core/metadata.hpp"

namespace nx::import_export {

/**
 * @brief Export format types
 */
enum class ExportFormat {
  Markdown,    // Individual markdown files
  Json,        // JSON with metadata
  Zip,         // ZIP archive
  Html,        // HTML files
  Pdf          // PDF files (requires additional tools)
};

/**
 * @brief Export options and configuration
 */
struct ExportOptions {
  ExportFormat format = ExportFormat::Markdown;
  std::filesystem::path output_path;
  bool include_metadata = true;
  bool preserve_structure = true;
  bool include_attachments = false;
  std::vector<std::string> tag_filter;      // Only export notes with these tags
  std::optional<std::string> notebook_filter; // Only export from this notebook
  std::optional<std::string> date_filter;     // Export notes since this date
  bool compress = false;                    // Compress output (for supported formats)
  std::string template_file;                // Custom template for HTML/PDF
};

/**
 * @brief Base exporter interface
 */
class Exporter {
public:
  virtual ~Exporter() = default;

  /**
   * @brief Export notes to the specified format
   * @param notes Notes to export
   * @param options Export configuration
   * @return Success or error
   */
  virtual Result<void> exportNotes(const std::vector<nx::core::Note>& notes, 
                                   const ExportOptions& options) = 0;

  /**
   * @brief Get supported file extensions for this format
   * @return List of file extensions (with dots)
   */
  virtual std::vector<std::string> getSupportedExtensions() const = 0;

  /**
   * @brief Get format description
   * @return Human-readable format description
   */
  virtual std::string getFormatDescription() const = 0;
};

/**
 * @brief Markdown exporter - exports notes as individual .md files
 */
class MarkdownExporter : public Exporter {
public:
  Result<void> exportNotes(const std::vector<nx::core::Note>& notes, 
                          const ExportOptions& options) override;

  std::vector<std::string> getSupportedExtensions() const override {
    return {".md", ".markdown"};
  }

  std::string getFormatDescription() const override {
    return "Markdown files with optional YAML front-matter";
  }

  /**
   * @brief Generate filename for a note
   * @param note Note to generate filename for
   * @param extension File extension to use
   * @return Generated filename
   */
  std::string generateFilename(const nx::core::Note& note, const std::string& extension) const;

private:

  /**
   * @brief Format note content with metadata
   * @param note Note to format
   * @param include_metadata Whether to include YAML front-matter
   * @return Formatted content
   */
  std::string formatNoteContent(const nx::core::Note& note, bool include_metadata) const;
};

/**
 * @brief JSON exporter - exports notes as structured JSON
 */
class JsonExporter : public Exporter {
public:
  Result<void> exportNotes(const std::vector<nx::core::Note>& notes, 
                          const ExportOptions& options) override;

  std::vector<std::string> getSupportedExtensions() const override {
    return {".json"};
  }

  std::string getFormatDescription() const override {
    return "JSON format with full metadata and content";
  }

private:
  /**
   * @brief Convert note to JSON object
   * @param note Note to convert
   * @return JSON representation
   */
  nlohmann::json noteToJson(const nx::core::Note& note) const;
};

/**
 * @brief ZIP exporter - creates compressed archive
 */
class ZipExporter : public Exporter {
public:
  explicit ZipExporter(std::unique_ptr<Exporter> base_exporter);

  Result<void> exportNotes(const std::vector<nx::core::Note>& notes, 
                          const ExportOptions& options) override;

  std::vector<std::string> getSupportedExtensions() const override {
    return {".zip"};
  }

  std::string getFormatDescription() const override {
    return "ZIP archive containing " + base_exporter_->getFormatDescription();
  }

private:
  std::unique_ptr<Exporter> base_exporter_;

  /**
   * @brief Create ZIP archive from directory
   * @param source_dir Directory to compress
   * @param zip_path Output ZIP file path
   * @return Success or error
   */
  Result<void> createZipArchive(const std::filesystem::path& source_dir,
                               const std::filesystem::path& zip_path) const;
};

/**
 * @brief HTML exporter - generates HTML files
 */
class HtmlExporter : public Exporter {
public:
  Result<void> exportNotes(const std::vector<nx::core::Note>& notes, 
                          const ExportOptions& options) override;

  std::vector<std::string> getSupportedExtensions() const override {
    return {".html", ".htm"};
  }

  std::string getFormatDescription() const override {
    return "HTML files with styling and navigation";
  }

  // Make these public so they can be used by PdfExporter
  std::string markdownToHtml(const std::string& markdown) const;
  std::string generateHtmlPage(const std::string& title, 
                              const std::string& content,
                              const std::string& template_file = "") const;

private:
  /**
   * @brief Generate index page with navigation
   * @param notes Notes to include in index
   * @return HTML index content
   */
  std::string generateIndexPage(const std::vector<nx::core::Note>& notes) const;
};

/**
 * @brief PDF exporter - generates PDF files using external tools
 */
class PdfExporter : public Exporter {
public:
  Result<void> exportNotes(const std::vector<nx::core::Note>& notes, 
                          const ExportOptions& options) override;

  std::vector<std::string> getSupportedExtensions() const override {
    return {".pdf"};
  }

  std::string getFormatDescription() const override {
    return "PDF files (requires pandoc+LaTeX, weasyprint, or wkhtmltopdf)";
  }

private:
  /**
   * @brief Check if required PDF generation tools are available
   * @return Tool name if available, empty string if none found
   */
  std::string findPdfTool() const;

  /**
   * @brief Convert markdown to PDF using pandoc
   * @param markdown_path Path to markdown file
   * @param pdf_path Output PDF path
   * @return Success or error
   */
  Result<void> convertWithPandoc(const std::filesystem::path& markdown_path,
                                const std::filesystem::path& pdf_path) const;

  /**
   * @brief Convert HTML to PDF using wkhtmltopdf
   * @param html_path Path to HTML file
   * @param pdf_path Output PDF path
   * @return Success or error
   */
  Result<void> convertWithWkhtmltopdf(const std::filesystem::path& html_path,
                                     const std::filesystem::path& pdf_path) const;

  /**
   * @brief Convert HTML to PDF using weasyprint
   * @param html_path Path to HTML file
   * @param pdf_path Output PDF path
   * @return Success or error
   */
  Result<void> convertWithWeasyprint(const std::filesystem::path& html_path,
                                    const std::filesystem::path& pdf_path) const;
};

/**
 * @brief Export manager - coordinates different export formats
 */
class ExportManager {
public:
  /**
   * @brief Create exporter for specified format
   * @param format Export format
   * @return Exporter instance or error
   */
  static Result<std::unique_ptr<Exporter>> createExporter(ExportFormat format);

  /**
   * @brief Export notes using the specified options
   * @param notes Notes to export
   * @param options Export configuration
   * @return Success or error
   */
  static Result<void> exportNotes(const std::vector<nx::core::Note>& notes,
                                 const ExportOptions& options);

  /**
   * @brief Filter notes based on export options
   * @param notes Input notes
   * @param options Export options with filters
   * @return Filtered notes
   */
  static std::vector<nx::core::Note> filterNotes(const std::vector<nx::core::Note>& notes,
                                                 const ExportOptions& options);

  /**
   * @brief Get list of supported export formats
   * @return Map of format to description
   */
  static std::map<ExportFormat, std::string> getSupportedFormats();

  /**
   * @brief Parse format string to ExportFormat enum
   * @param format_string Format name (e.g., "markdown", "json")
   * @return ExportFormat or error
   */
  static Result<ExportFormat> parseFormat(const std::string& format_string);

private:
  /**
   * @brief Parse date filter string into start/end time points
   * @param date_filter Date filter string (e.g., "2024-01-01", "after:2024-01-01", "2024-01-01:2024-12-31")
   * @return Pair of optional start and end dates, or nullopt if parsing failed
   */
  static std::optional<std::pair<std::optional<std::chrono::system_clock::time_point>, 
                                std::optional<std::chrono::system_clock::time_point>>> 
    parseDateFilter(const std::string& date_filter);

  /**
   * @brief Parse ISO date string (YYYY-MM-DD) to time_point
   * @param date_str Date string to parse
   * @return time_point or nullopt if parsing failed
   */
  static std::optional<std::chrono::system_clock::time_point> 
    parseIsoDate(const std::string& date_str);
};

} // namespace nx::import_export