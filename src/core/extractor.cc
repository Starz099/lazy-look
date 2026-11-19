#include "extractor.hh"

#include "pipeline/document_handler.hh"
#include "pipeline/media_handler.hh"
#include "pipeline/registry.hh"

namespace lazyfinder
{

bool Extractor::is_supported(const std::filesystem::path &path)
{
  return HandlerRegistry::instance().is_supported(path);
}

std::vector<std::string> Extractor::extract_chunks(const std::filesystem::path &path)
{
  auto *handler = HandlerRegistry::instance().find_handler(path);
  if (!handler) {
    return {};
  }

  auto processed = handler->process(path);
  std::vector<std::string> chunks;
  chunks.reserve(processed.chunks.size());
  for (auto &chunk : processed.chunks) {
    chunks.push_back(std::move(chunk.text));
  }
  return chunks;
}

std::vector<std::string> Extractor::extract_text_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_text(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_html_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_html(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_pdf_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_pdf(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_docx_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_docx(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_xlsx_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_xlsx(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_pptx_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_pptx(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_opendocument_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_opendocument(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_epub_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_epub(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_rtf_file(const std::filesystem::path &path)
{
  auto chunks = DocumentHandler::extract_rtf(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_subtitle_file(const std::filesystem::path &path)
{
  auto chunks = MediaHandler::extract_subtitle_file(path);
  std::vector<std::string> res;
  res.reserve(chunks.size());
  for (auto &c : chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

std::vector<std::string> Extractor::extract_media_file(const std::filesystem::path &path)
{
  MediaHandler handler;
  auto processed = handler.process(path);
  std::vector<std::string> res;
  res.reserve(processed.chunks.size());
  for (auto &c : processed.chunks) {
    res.push_back(std::move(c.text));
  }
  return res;
}

} // namespace lazyfinder
