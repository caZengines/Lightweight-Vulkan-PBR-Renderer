#include "rhi/descriptor_writer.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace rhi {

namespace {

[[noreturn]] void fail(const std::string& what) {
    throw std::runtime_error("rhi::DescriptorWriter: " + what);
}

bool acceptsImage(vk::DescriptorType type) {
    switch (type) {
        case vk::DescriptorType::eSampler:
        case vk::DescriptorType::eCombinedImageSampler:
        case vk::DescriptorType::eSampledImage:
        case vk::DescriptorType::eStorageImage:
        case vk::DescriptorType::eInputAttachment:
            return true;
        default:
            return false;
    }
}

bool acceptsBuffer(vk::DescriptorType type) {
    switch (type) {
        case vk::DescriptorType::eUniformBuffer:
        case vk::DescriptorType::eStorageBuffer:
        case vk::DescriptorType::eUniformBufferDynamic:
        case vk::DescriptorType::eStorageBufferDynamic:
            return true;
        default:
            return false;
    }
}

}  // namespace

DescriptorWriter::DescriptorWriter(vk::raii::Device& device,
                                   vk::DescriptorSet set,
                                   std::span<const vk::DescriptorSetLayoutBinding> table,
                                   std::span<const std::string> names)
    : device_(&device), set_(set), table_(table), names_(names) {
    if (set_ == nullptr) {
        fail("constructed with a null descriptor set");
    }
    if (names_.size() != table_.size()) {
        fail("binding table and name table are not parallel (" + std::to_string(table_.size()) +
             " bindings vs " + std::to_string(names_.size()) + " names)");
    }
}

DescriptorWriter::Resolved DescriptorWriter::resolve(std::string_view name, bool forImage) const {
    for (size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] != name) continue;

        const vk::DescriptorType type = table_[i].descriptorType;
        if (!(forImage ? acceptsImage(type) : acceptsBuffer(type))) {
            fail("binding '" + std::string(name) + "' is " + vk::to_string(type) +
                 ", which does not accept a " + (forImage ? "image" : "buffer") + " write");
        }
        return Resolved{table_[i].binding, type};
    }

    std::string available;
    for (const auto& n : names_) {
        if (n.empty()) continue;
        if (!available.empty()) available += ", ";
        available += n;
    }
    fail("no binding named '" + std::string(name) + "'; available: " +
         (available.empty() ? std::string("(none)") : available));
}

uint32_t DescriptorWriter::reserveBinding(std::string_view name, const Resolved& resolved) {
    if (std::ranges::find(writtenBindings_, resolved.binding) != writtenBindings_.end()) {
        fail("binding '" + std::string(name) + "' written twice in one flush");
    }
    writtenBindings_.emplace_back(resolved.binding);
    return resolved.binding;
}

DescriptorWriter& DescriptorWriter::writeImage(std::string_view name,
                                               const vk::DescriptorImageInfo& info) {
    if (flushed_) fail("write after flush()");

    const Resolved resolved = resolve(name, /*forImage=*/true);
    reserveBinding(name, resolved);

    images_.emplace_back(info);   // stable address: the write points here
    vk::WriteDescriptorSet write{};
    write.setDstSet(set_)
         .setDstBinding(resolved.binding)
         .setDescriptorType(resolved.type)
         .setImageInfo(images_.back());
    writes_.emplace_back(write);
    return *this;
}

DescriptorWriter& DescriptorWriter::writeBuffer(std::string_view name,
                                                const vk::DescriptorBufferInfo& info) {
    if (flushed_) fail("write after flush()");

    const Resolved resolved = resolve(name, /*forImage=*/false);
    reserveBinding(name, resolved);

    buffers_.emplace_back(info);   // stable address: the write points here
    vk::WriteDescriptorSet write{};
    write.setDstSet(set_)
         .setDstBinding(resolved.binding)
         .setDescriptorType(resolved.type)
         .setBufferInfo(buffers_.back());
    writes_.emplace_back(write);
    return *this;
}

void DescriptorWriter::flush() {
    if (flushed_) fail("flush() called twice");
    if (!writes_.empty()) {
        device_->updateDescriptorSets(writes_, {});
    }
    flushed_ = true;
}

}  // namespace rhi
