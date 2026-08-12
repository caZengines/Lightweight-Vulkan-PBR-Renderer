#pragma once
#include "generic/mesh.hpp"
#include "command_manager.hpp"
class glTFModel {
    public:
        //ban copying
        glTFModel(const glTFModel&) = delete;
        glTFModel& operator=(const glTFModel&) = delete;

        static std::unique_ptr<glTFModel> fromglTF(const std::string& modelPath,
                                                   VmaAllocator* alloc, CommandPool& commandPool);
    private:
        std::unique_ptr<Mesh>               mesh_ = nullptr;
};