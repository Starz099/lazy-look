#include "pipeline/registry.hh"

#include "pipeline/document_handler.hh"
#include "pipeline/image_handler.hh"
#include "pipeline/media_handler.hh"

namespace lazylook
{

HandlerRegistry::HandlerRegistry()
{
  register_handler(std::make_unique<DocumentHandler>());
  register_handler(std::make_unique<ImageHandler>());
  register_handler(std::make_unique<MediaHandler>());
}

HandlerRegistry::~HandlerRegistry() = default;

HandlerRegistry::HandlerRegistry(HandlerRegistry &&) noexcept = default;
HandlerRegistry &HandlerRegistry::operator=(HandlerRegistry &&) noexcept = default;

void HandlerRegistry::register_handler(std::unique_ptr<IFileHandler> handler)
{
  if (handler) {
    handlers_.push_back(std::move(handler));
  }
}

IFileHandler *HandlerRegistry::find_handler(const std::filesystem::path &path) const
{
  for (const auto &handler : handlers_) {
    if (handler && handler->supports(path)) {
      return handler.get();
    }
  }
  return nullptr;
}

bool HandlerRegistry::is_supported(const std::filesystem::path &path) const
{
  return find_handler(path) != nullptr;
}

FileCategory HandlerRegistry::get_category(const std::filesystem::path &path) const
{
  const auto *handler = find_handler(path);
  if (handler) {
    return handler->category();
  }
  return FileCategory::Document;
}

HandlerRegistry &HandlerRegistry::instance()
{
  static HandlerRegistry reg;
  return reg;
}

} // namespace lazylook
