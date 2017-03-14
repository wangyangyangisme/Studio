#include "meshpropertywidget.h"
#include "../filepickerwidget.h"
#include "../../irisgl/src/scenegraph/meshnode.h"
#include "../../globals.h"
#include "../sceneviewwidget.h"

MeshPropertyWidget::MeshPropertyWidget()
{
    meshPicker = this->addFilePicker("Mesh Path");

    connect(meshPicker, SIGNAL(onPathChanged(QString)), SLOT(onMeshPathChanged(QString)));
}

MeshPropertyWidget::~MeshPropertyWidget()
{

}

void MeshPropertyWidget::onMeshPathChanged(const QString &path)
{
    Globals::sceneViewWidget->makeCurrent();
    meshNode->setMesh(path);
    Globals::sceneViewWidget->doneCurrent();
}

void MeshPropertyWidget::setSceneNode(iris::SceneNodePtr sceneNode)
{
    if (!!sceneNode && sceneNode->sceneNodeType == iris::SceneNodeType::Mesh) {
        this->meshNode = sceneNode.staticCast<iris::MeshNode>();
        meshPicker->setFilepath(meshNode->meshPath);
    } else {
        this->meshNode.clear();
    }
}
