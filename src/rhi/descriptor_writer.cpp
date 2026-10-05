#include "rhi/descriptor_writer.hpp"

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

std::string describeBinding(const DescriptorBinding& binding) {
    return "binding " + std::to_string(binding.vk.binding) + " ('" +
           (binding.name.empty() ? std::string("<unnamed>") : binding.name) + "', " +
           vk::to_string(binding.vk.descriptorType) + ")";
}

}  // namespace

DescriptorWriter::DescriptorWriter(vk::raii::Device& device,
                                   vk::DescriptorSet set,
                                   std::span<const DescriptorBinding> table)
    : device_(&device), set_(set), table_(table) {
    if (set_ == nullptr) {
        fail("constructed with a null descriptor set");
    }
}

size_t DescriptorWriter::indexOfName(std::string_view name) const {
    for (size_t i = 0; i < table_.size(); ++i) {
        // Empty names are not addressable: hand-made layouts (set 0) are written
        // by number, deliberately.
        if (!table_[i].name.empty() && table_[i].name == name) return i;
    }

    std::string available;
    for (const auto& binding : table_) {
        if (binding.name.empty()) continue;
        if (!available.empty()) available += ", ";
        available += binding.name;
    }
    fail("no binding named '" + std::string(name) + "'; available: " +
         (available.empty() ? std::string("(none)") : available));
}

void DescriptorWriter::apply(const DescriptorBinding& binding, const DescriptorWrite& value) {
    if (const auto* image = std::get_if<ImageWrite>(&value)) {
        if (!acceptsImage(binding.vk.descriptorType)) {
            fail(describeBinding(binding) + " does not accept an image write");
        }
        imageArrays_.emplace_back(image->infos.begin(), image->infos.end());
        vk::WriteDescriptorSet write{};
        write.setDstSet(set_)
             .setDstBinding(binding.vk.binding)
             .setDescriptorType(binding.vk.descriptorType)
             .setImageInfo(imageArrays_.back());   // sets descriptorCount too
        writes_.emplace_back(write);
        return;
    }

    const auto* buffer = std::get_if<BufferWrite>(&value);
    if (buffer == nullptr) {
        fail("value for " + describeBinding(binding) + " is neither an image nor a buffer write");
    }
    if (!acceptsBuffer(binding.vk.descriptorType)) {
        fail(describeBinding(binding) + " does not accept a buffer write");
    }
    bufferArrays_.emplace_back(buffer->infos.begin(), buffer->infos.end());
    vk::WriteDescriptorSet write{};
    write.setDstSet(set_)
         .setDstBinding(binding.vk.binding)
         .setDescriptorType(binding.vk.descriptorType)
         .setBufferInfo(bufferArrays_.back());
    writes_.emplace_back(write);
}

void DescriptorWriter::writeAll(std::span<const DescriptorWrite> values) {
    if (flushed_) fail("writeAll() called after flush()");

    constexpr size_t kUnbound = static_cast<size_t>(-1);
    std::vector<size_t> valueOf(table_.size(), kUnbound);

    for (size_t v = 0; v < values.size(); ++v) {
        const std::string_view name = nameOf(values[v]);
        const size_t slot = indexOfName(name);
        if (valueOf[slot] != kUnbound) {
            fail("two values for binding '" + std::string(name) + "'");
        }
        const uint32_t expected = table_[slot].vk.descriptorCount;
        if (countOf(values[v]) != expected) {
            fail("value for binding '" + std::string(name) + "' supplies " +
                 std::to_string(countOf(values[v])) + " descriptor(s) but the layout declares " +
                 std::to_string(expected));
        }
        valueOf[slot] = v;
    }

    for (size_t slot = 0; slot < table_.size(); ++slot) {
        if (valueOf[slot] == kUnbound) {
            fail(describeBinding(table_[slot]) +
                 " has no value; every binding in the layout must be written");
        }
        apply(table_[slot], values[valueOf[slot]]);
    }
}

void DescriptorWriter::flush() {
    if (flushed_) fail("flush() called twice");
    if (writes_.empty()) fail("flush() with nothing written");
    device_->updateDescriptorSets(writes_, {});
    flushed_ = true;
}

}  // namespace rhi
