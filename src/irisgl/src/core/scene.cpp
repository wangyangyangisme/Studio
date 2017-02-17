/**************************************************************************
This file is part of IrisGL
http://www.irisgl.org
Copyright (c) 2016  GPLv3 Jahshaka LLC <coders@jahshaka.com>

This is free software: you may copy, redistribute
and/or modify it under the terms of the GPLv3 License

For more information see the LICENSE file
*************************************************************************/

#include "scene.h"
#include "scenenode.h"
#include "../scenegraph/lightnode.h"
#include "../scenegraph/cameranode.h"
#include "../scenegraph/viewernode.h"
#include "../graphics/mesh.h"
#include "../graphics/renderitem.h"
#include "../materials/defaultskymaterial.h"
#include "irisutils.h"

namespace iris
{

Scene::Scene()
{
    rootNode = SceneNode::create();
    rootNode->setName("World");
    // rootNode->setScene(this->sharedFromThis());

    // todo: move this to ui code
    skyMesh = Mesh::loadMesh(IrisUtils::getAbsoluteAssetPath("app/content/primitives/sky.obj"));
    // skyTexture = Texture2D::load("app/content/skies/default.png");
    skyMaterial = DefaultSkyMaterial::create();
    skyColor = QColor(255, 255, 255, 255);
    skyRenderItem = new RenderItem();
    skyRenderItem->mesh = skyMesh;
    skyRenderItem->material = skyMaterial;
    skyRenderItem->type = RenderItemType::Mesh;
    skyRenderItem->renderLayer = (int)RenderLayer::Background;

    fogColor = QColor(250, 250, 250);
    fogStart = 100;
    fogEnd = 180;
    fogEnabled = true;

    ambientColor = QColor(64, 64, 64);

    //reserve 1000 items initially
    geometryRenderList.reserve(1000);
    shadowRenderList.reserve(1000);
}

void Scene::setSkyTexture(Texture2DPtr tex)
{
    skyTexture = tex;
    skyMaterial->setSkyTexture(tex);
}

QString Scene::getSkyTextureSource()
{
    return skyTexture->getSource();
}

void Scene::clearSkyTexture()
{
    skyTexture.clear();
    skyMaterial->clearSkyTexture();
}

void Scene::setSkyColor(QColor color)
{
    this->skyColor = color;
    skyMaterial->setSkyColor(color);
}

void Scene::setAmbientColor(QColor color)
{
    this->ambientColor = color;
}

void Scene::updateSceneAnimation(float time)
{
    rootNode->updateAnimation(time);
}

void Scene::update(float dt)
{
    rootNode->update(dt);

    // cameras aren't always be a part of the scene hierarchy, so their matrices are updated here
    if (!!camera) {
        camera->update(dt);
        camera->updateCameraMatrices();
    }

    this->geometryRenderList.append(skyRenderItem);
}

void Scene::render()
{

}

void Scene::addNode(SceneNodePtr node)
{
    if (node->sceneNodeType == SceneNodeType::Light) {
        auto light = node.staticCast<iris::LightNode>();
        lights.append(light);
    }

    if(node->sceneNodeType == SceneNodeType::Viewer && vrViewer.isNull())
    {
        vrViewer = node.staticCast<iris::ViewerNode>();
    }
}

void Scene::removeNode(SceneNodePtr node)
{
    if (node->sceneNodeType == SceneNodeType::Light) {
        lights.removeOne(node.staticCast<iris::LightNode>());
    }

    // If this node is the scene's viewer then reset the scene's viewer to null
    if(vrViewer == node.staticCast<iris::ViewerNode>())
        vrViewer.reset();
}

void Scene::setCamera(CameraNodePtr cameraNode)
{
    camera = cameraNode;
}

ScenePtr Scene::create()
{
    ScenePtr scene(new Scene());
    scene->rootNode->setScene(scene);

    return scene;
}

void Scene::setOutlineWidth(int width)
{
    outlineWidth = width;
}

void Scene::setOutlineColor(QColor color)
{
    outlineColor = color;
}

}
