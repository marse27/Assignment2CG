#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <functional>

struct SceneNode {
    glm::mat4 local{1.0f};
    glm::mat4 world{1.0f};
    std::vector<SceneNode*> children;

    explicit SceneNode(const glm::mat4& L = glm::mat4(1.0f)) : local(L) {}

    ~SceneNode() {
        for (auto* c : children) delete c;
    }

    SceneNode* addChild(SceneNode* c) {
        children.push_back(c);
        return c;
    }

    // propagate transforms
    void update(const glm::mat4& parentWorld = glm::mat4(1.0f)) {
        world = parentWorld * local;
        for (auto* c : children) c->update(world);
    }

    // depth-first draw
    void traverse(const std::function<void(const glm::mat4&)>& drawFn) const {
        drawFn(world);
        for (auto* c : children) c->traverse(drawFn);
    }
};
