/**************************************************************************
This file is part of JahshakaVR, VR Authoring Toolkit
http://www.jahshaka.com
Copyright (c) 2016  GPLv3 Jahshaka LLC <coders@jahshaka.com>

This is free software: you may copy, redistribute
and/or modify it under the terms of the GPLv3 License

For more information see the LICENSE file
*************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <qwindow.h>
#include <qsurface.h>
#include <QScrollArea>

//#include "irisgl/src/irisgl.h"
#include "irisgl/src/scenegraph/meshnode.h"
#include "irisgl/src/scenegraph/cameranode.h"
#include "irisgl/src/scenegraph/scene.h"
#include "irisgl/src/scenegraph/scenenode.h"
#include "irisgl/src/scenegraph/lightnode.h"
#include "irisgl/src/scenegraph/viewernode.h"
#include "irisgl/src/scenegraph/particlesystemnode.h"
#include "irisgl/src/scenegraph/meshnode.h"
#include "irisgl/src/materials/defaultmaterial.h"
#include "irisgl/src/materials/custommaterial.h"
#include "irisgl/src/graphics/forwardrenderer.h"
#include "irisgl/src/graphics/mesh.h"
#include "irisgl/src/graphics/shader.h"
#include "irisgl/src/graphics/texture2d.h"
#include "irisgl/src/graphics/viewport.h"
#include "irisgl/src/graphics/texture2d.h"
#include "irisgl/src/animation/keyframeset.h"
#include "irisgl/src/animation/keyframeanimation.h"
#include "irisgl/src/animation/animation.h"
#include "irisgl/src/graphics/postprocessmanager.h"
#include "irisgl/src/core/logger.h"
#include "src/dialogs/donatedialog.h"

#include <QFontDatabase>
#include <QOpenGLContext>
#include <qstandarditemmodel.h>
#include <QKeyEvent>
#include <QMessageBox>
#include <QOpenGLDebugLogger>
#include <QUndoStack>

#include <QBuffer>
#include <QDirIterator>
#include <QDockWidget>
#include <QFileDialog>
#include <QTemporaryDir>

#include <QTreeWidgetItem>

#include <QPushButton>
#include <QTimer>
#include <math.h>
#include <QDesktopServices>
#include <QShortcut>

#include "dialogs/loadmeshdialog.h"
#include "core/surfaceview.h"
#include "core/nodekeyframeanimation.h"
#include "core/nodekeyframe.h"
#include "globals.h"

#include "widgets/animationwidget.h"

#include "dialogs/renamelayerdialog.h"
#include "widgets/layertreewidget.h"
#include "core/project.h"
#include "widgets/accordianbladewidget.h"

#include "editor/editorcameracontroller.h"
#include "core/settingsmanager.h"
#include "dialogs/preferencesdialog.h"
#include "dialogs/preferences/worldsettings.h"
#include "dialogs/licensedialog.h"
#include "dialogs/aboutdialog.h"

#include "helpers/collisionhelper.h"

#include "widgets/sceneviewwidget.h"
#include "core/materialpreset.h"
#include "widgets/postprocesseswidget.h"

#include "widgets/projectmanager.h"

#include "io/scenewriter.h"
#include "io/scenereader.h"

#include "constants.h"
#include <src/io/materialreader.hpp>
#include "uimanager.h"
#include "core/database/database.h"

#include "commands/addscenenodecommand.h"
#include "commands/deletescenenodecommand.h"

#include "widgets/screenshotwidget.h"
#include "editor/editordata.h"
#include "widgets/assetwidget.h"

#include "../src/dialogs/newprojectdialog.h"

#include "../src/widgets/scenehierarchywidget.h"
#include "../src/widgets/scenenodepropertieswidget.h"

#include "../src/widgets/materialsets.h"
#include "../src/widgets/modelpresets.h"
#include "../src/widgets/skypresets.h"

#include "src/irisgl/src/zip/zip.h"

enum class VRButtonMode : int
{
    Default = 0,
    Disabled,
    VRMode
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Jahshaka " + Constants::CONTENT_VERSION);

    UiManager::mainWindow = this;

    QFont font;
    font.setFamily(font.defaultFamily());
    font.setPointSize(font.pointSize() * devicePixelRatio());
    setFont(font);

#ifdef QT_DEBUG
    iris::Logger::getSingleton()->init(getAbsoluteAssetPath("jahshaka.log"));
    setWindowTitle(windowTitle() + " - Developer Build");
#else
    iris::Logger::getSingleton()->init(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/jahshaka.log");
#endif

    createPostProcessDockWidget();

    settings = SettingsManager::getDefaultManager();
    prefsDialog = new PreferencesDialog(settings);
    aboutDialog = new AboutDialog();
    licenseDialog = new LicenseDialog();

    camControl = nullptr;
    vrMode = false;

    setupFileMenu();
    setupHelpMenu();

    setupViewPort();
    setupProjectDB();
    setupDesktop();
    setupToolBar();
    setupDockWidgets();
    setupShortcuts();

//    if (!UiManager::playMode) {
//        restoreGeometry(settings->getValue("geometry", "").toByteArray());
//        restoreState(settings->getValue("windowState", "").toByteArray());
//    }

    setupUndoRedo();

    // this ties to hidden geometry so should come at the end
    setupViewMenu();
}

void MainWindow::grabOpenGLContextHack()
{
    switchSpace(WindowSpaces::PLAYER);
}

void MainWindow::goToDesktop()
{
    showMaximized();
    switchSpace(WindowSpaces::DESKTOP);
}

void MainWindow::setupVrUi()
{
    vrButton->setToolTipDuration(0);

    if (sceneView->isVrSupported()) {
        vrButton->setEnabled(true);
        vrButton->setToolTip("Press to view the scene in vr");
        vrButton->setProperty("vrMode", (int) VRButtonMode::Default);
    } else {
        vrButton->setEnabled(false);
        vrButton->setToolTip("No Oculus device detected");
        vrButton->setProperty("vrMode", (int) VRButtonMode::Disabled);
    }

    connect(vrButton, SIGNAL(clicked(bool)), SLOT(vrButtonClicked(bool)));

    // needed to apply changes
    vrButton->style()->unpolish(vrButton);
    vrButton->style()->polish(vrButton);
}

/**
 * uses style property trick
 * http://wiki.qt.io/Dynamic_Properties_and_Stylesheets
 */
void MainWindow::vrButtonClicked(bool)
{
    if (!sceneView->isVrSupported()) {
        // pass
    } else {
        if (sceneView->getViewportMode()==ViewportMode::Editor) {
            sceneView->setViewportMode(ViewportMode::VR);

            // highlight button blue
            vrButton->setProperty("vrMode",(int)VRButtonMode::VRMode);
        } else {
            sceneView->setViewportMode(ViewportMode::Editor);

            // return button back to normal color
            vrButton->setProperty("vrMode",(int)VRButtonMode::Default);
        }
    }

    // needed to apply changes
    vrButton->style()->unpolish(vrButton);
    vrButton->style()->polish(vrButton);
}

iris::ScenePtr MainWindow::getScene()
{
    return scene;
}

QString MainWindow::getAbsoluteAssetPath(QString pathRelativeToApp)
{
    QDir basePath = QDir(QCoreApplication::applicationDirPath());

#if defined(WIN32) && defined(QT_DEBUG)
    basePath.cdUp();
#elif defined(Q_OS_MAC)
    basePath.cdUp();
    basePath.cdUp();
    basePath.cdUp();
#endif

    auto path = QDir::cleanPath(basePath.absolutePath() + QDir::separator() + pathRelativeToApp);
    return path;
}

iris::ScenePtr MainWindow::createDefaultScene()
{
    auto scene = iris::Scene::create();

    scene->setSkyColor(QColor(72, 72, 72));
    scene->setAmbientColor(QColor(96, 96, 96));

    // second node
    auto node = iris::MeshNode::create();
    node->setMesh(":/models/ground.obj");
    node->setLocalPos(QVector3D(0, 1e-4, 0)); // prevent z-fighting with the default plane (iKlsR)
    node->setName("Ground");
    node->setPickable(false);
    node->setShadowEnabled(false);

    auto m = iris::CustomMaterial::create();
    m->generate(IrisUtils::getAbsoluteAssetPath(Constants::DEFAULT_SHADER));
    m->setValue("diffuseTexture", ":/content/textures/tile.png");
    m->setValue("textureScale", 4.f);
    node->setMaterial(m);

    scene->rootNode->addChild(node);

    auto dlight = iris::LightNode::create();
    dlight->setLightType(iris::LightType::Directional);
    scene->rootNode->addChild(dlight);
    dlight->setName("Directional Light");
    dlight->setLocalPos(QVector3D(4, 4, 0));
    dlight->setLocalRot(QQuaternion::fromEulerAngles(15, 0, 0));
    dlight->intensity = 1;
    dlight->icon = iris::Texture2D::load(":/icons/light.png");

    auto plight = iris::LightNode::create();
    plight->setLightType(iris::LightType::Point);
    scene->rootNode->addChild(plight);
    plight->setName("Point Light");
    plight->setLocalPos(QVector3D(-4, 4, 0));
    plight->intensity = 1;
    plight->icon = iris::Texture2D::load(":/icons/bulb.png");

    // fog params
    scene->fogColor = QColor(72, 72, 72);
    scene->shadowEnabled = true;

    sceneNodeSelected(scene->rootNode);

    return scene;
}

void MainWindow::initializeGraphics(SceneViewWidget *widget, QOpenGLFunctions_3_2_Core *gl)
{
    Q_UNUSED(gl);
    postProcessWidget->setPostProcessMgr(widget->getRenderer()->getPostProcessManager());
    setupVrUi();
}

void MainWindow::setSettingsManager(SettingsManager* settings)
{
    this->settings = settings;
}

SettingsManager* MainWindow::getSettingsManager()
{
    return settings;
}

bool MainWindow::handleMousePress(QMouseEvent *event)
{
    mouseButton = event->button();
    mousePressPos = event->pos();

    return true;
}

bool MainWindow::handleMouseRelease(QMouseEvent *event)
{
    return true;
}

bool MainWindow::handleMouseMove(QMouseEvent *event)
{
    mousePos = event->pos();
    return false;
}

// TODO - disable scrolling while doing gizmo transform ?
bool MainWindow::handleMouseWheel(QWheelEvent *event)
{
    return false;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
        case QEvent::DragMove: {
            auto evt = static_cast<QDragMoveEvent*>(event);
            if (obj == sceneContainer) {
                auto info = QFileInfo(evt->mimeData()->text());

                if (!!activeSceneNode) {
                    sceneView->hideGizmo();

                    if (Constants::MODEL_EXTS.contains(info.suffix())) {
                        if (sceneView->doActiveObjectPicking(evt->posF())) {
                            //activeSceneNode->pos = sceneView->hit;
                            dragScenePos = sceneView->hit;
                        } else if (sceneView->updateRPI(sceneView->editorCam->getLocalPos(),
                                                        sceneView->calculateMouseRay(evt->posF())))
                        {
                            //activeSceneNode->pos = sceneView->Offset;
                            dragScenePos = sceneView->Offset;
                        } else {
                            ////////////////////////////////////////
                            const float spawnDist = 10.0f;
                            auto offset = sceneView->editorCam->getLocalRot().rotatedVector(QVector3D(0, -1.0f, -spawnDist));
                            offset += sceneView->editorCam->getLocalPos();
                            //activeSceneNode->pos = offset;
                            dragScenePos = offset;
                        }
                    }
                }

                if (!Constants::MODEL_EXTS.contains(info.suffix())) {
                    sceneView->doObjectPicking(evt->posF(), iris::SceneNodePtr(), false, true);
                }
            }
        }

        case QEvent::DragEnter: {
            auto evt = static_cast<QDragEnterEvent*>(event);

            sceneView->hideGizmo();

            if (obj == sceneContainer) {
                if (evt->mimeData()->hasText()) {
                    evt->acceptProposedAction();
                } else {
                    evt->ignore();
                }

                auto info = QFileInfo(evt->mimeData()->text());
                if (Constants::MODEL_EXTS.contains(info.suffix())) {

                    if (dragging) {
                        // TODO swap this with the actual model later on
                        addDragPlaceholder();
                        dragging = !dragging;
                    }
                }
            }

            break;
        }

        case QEvent::Drop: {
            if (obj == sceneContainer) {
                auto evt = static_cast<QDropEvent*>(event);

                auto info = QFileInfo(evt->mimeData()->text());
                if (evt->mimeData()->hasText()) {
                    evt->acceptProposedAction();
                } else {
                    evt->ignore();
                }

                if (Constants::MODEL_EXTS.contains(info.suffix())) {
                    //auto ppos = activeSceneNode->pos;
                    auto ppos = dragScenePos;
                    //deleteNode();
                    addMesh(evt->mimeData()->text(), true, ppos);
                }

                if (!!activeSceneNode && !Constants::MODEL_EXTS.contains(info.suffix())) {
                    auto meshNode = activeSceneNode.staticCast<iris::MeshNode>();
                    auto mat = meshNode->getMaterial().staticCast<iris::CustomMaterial>();

                    if (!mat->firstTextureSlot().isEmpty()) {
                        mat->setValue(mat->firstTextureSlot(), evt->mimeData()->text());
                    }
                }
            }

            break;
        }

        case QEvent::MouseButtonPress: {
            dragging = true;

            if (obj == surface) return handleMousePress(static_cast<QMouseEvent*>(event));

            if (obj == sceneContainer) {
                sceneView->mousePressEvent(static_cast<QMouseEvent*>(event));
            }

            break;
        }

        case QEvent::MouseButtonRelease: {
            if (obj == surface) return handleMouseRelease(static_cast<QMouseEvent*>(event));
            break;
        }

        case QEvent::MouseMove: {
            if (obj == surface) return handleMouseMove(static_cast<QMouseEvent*>(event));
            break;
        }

        case QEvent::Wheel: {
            if (obj == surface) return handleMouseWheel(static_cast<QWheelEvent*>(event));
            break;
        }

        default:
            break;
    }

    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    bool closing = false;

    if (UiManager::isUndoStackDirty()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this,
                                      "Unsaved Changes",
                                      "There are unsaved changes, save before closing?",
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            saveScene();
            event->accept();
            closing = true;
        } else if (reply == QMessageBox::No) {
            event->accept();
            closing = true;
        } else {
            event->ignore();
            return;
        }
    } else {
        event->accept();
        closing = true;
    }

//#ifndef QT_DEBUG
    if (closing) {
        if (!getSettingsManager()->getValue("ddialog_seen", "false").toBool()) {
            DonateDialog dialog;
            dialog.exec();
        }
    }
//#endif

//    if (!UiManager::playMode) {
//        settings->setValue("geometry", saveGeometry());
//        settings->setValue("windowState", saveState());
//    }

    ThumbnailGenerator::getSingleton()->shutdown();
}

void MainWindow::setupFileMenu()
{
    connect(ui->actionSave,         SIGNAL(triggered(bool)), this, SLOT(saveScene()));
    connect(ui->actionExit,         SIGNAL(triggered(bool)), this, SLOT(exitApp()));
    connect(ui->actionPreferences,  SIGNAL(triggered(bool)), this, SLOT(showPreferences()));
    connect(prefsDialog,            SIGNAL(PreferencesDialogClosed()), SLOT(updateSceneSettings()));
    connect(ui->actionExport,       SIGNAL(triggered(bool)), this, SLOT(exportSceneAsZip()));
    connect(ui->actionClose,        &QAction::triggered, [this](bool) { closeProject(); });
}

void MainWindow::setupViewMenu()
{
    connect(ui->actionOutliner, &QAction::toggled, [this](bool set) {
        sceneHierarchyDock->setVisible(set);
    });

    connect(ui->actionProperties, &QAction::toggled, [this](bool set) {
        sceneNodePropertiesDock->setVisible(set);
    });

    connect(ui->actionPresets, &QAction::toggled, [this](bool set) {
        presetsDock->setVisible(set);
    });

    connect(ui->actionAnimation, &QAction::toggled, [this](bool set) {
        animationDock->setVisible(set);
    });

    connect(ui->actionAssets, &QAction::toggled, [this](bool set) {
        assetDock->setVisible(set);
    });

    connect(ui->actionClose_All, &QAction::triggered, [this]() {
        toggleWidgets(false);
    });

    connect(ui->actionRestore_All, &QAction::triggered, [this]() {
        toggleWidgets(true);
    });
}

void MainWindow::setupHelpMenu()
{
    connect(ui->actionLicense,      SIGNAL(triggered(bool)), this, SLOT(showLicenseDialog()));
    connect(ui->actionAbout,        SIGNAL(triggered(bool)), this, SLOT(showAboutDialog()));
    connect(ui->actionFacebook,     SIGNAL(triggered(bool)), this, SLOT(openFacebookUrl()));
    connect(ui->actionOpenWebsite,  SIGNAL(triggered(bool)), this, SLOT(openWebsiteUrl()));
}

void MainWindow::createPostProcessDockWidget()
{
    postProcessDockWidget = new QDockWidget(this);
    postProcessWidget = new PostProcessesWidget();
    // postProcessWidget->setWindowTitle("Post Processes");
    postProcessDockWidget->setWidget(postProcessWidget);
    postProcessDockWidget->setWindowTitle("PostProcesses");
    // postProcessDockWidget->setFloating(true);
    postProcessDockWidget->setHidden(true);
    this->addDockWidget(Qt::RightDockWidgetArea, postProcessDockWidget);

}

void MainWindow::sceneTreeCustomContextMenu(const QPoint& pos)
{
}

void MainWindow::renameNode()
{
    RenameLayerDialog dialog(this);
    dialog.setName(activeSceneNode->getName());
    dialog.exec();

    activeSceneNode->setName(dialog.getName());
    this->sceneHierarchyWidget->repopulateTree();
}

void MainWindow::stopAnimWidget()
{
    animWidget->stopAnimation();
}

void MainWindow::setupProjectDB()
{
    auto path = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation))
                .filePath(Constants::JAH_DATABASE);

    db = new Database();
    db->initializeDatabase(path);
    db->createGlobalDb();
    db->createGlobalDbThumbs();
}

void MainWindow::setupUndoRedo()
{
    undoStack = new QUndoStack(this);
    UiManager::setUndoStack(undoStack);
    UiManager::mainWindow = this;

    connect(ui->actionUndo, &QAction::triggered, [this]() {
        undo();
        UiManager::updateWindowTitle();
    });

    connect(ui->actionEditUndo, &QAction::triggered, [this]() {
        undo();
        UiManager::updateWindowTitle();
    });

    ui->actionEditUndo->setShortcuts(QKeySequence::Undo);

    connect(ui->actionRedo, &QAction::triggered, [this]() {
        redo();
        UiManager::updateWindowTitle();
    });

    connect(ui->actionEditRedo, &QAction::triggered, [this]() {
        redo();
        UiManager::updateWindowTitle();
    });

    ui->actionEditRedo->setShortcuts(QKeySequence::Redo);
}

void MainWindow::switchSpace(WindowSpaces space)
{
   const QString disabledMenu   = "color: #444; border-color: #111";
   const QString selectedMenu   = "border-color: white";
   const QString unselectedMenu = "border-color: #111";

    switch (currentSpace = space) {
        case WindowSpaces::DESKTOP: {
            if (UiManager::isSceneOpen) pmContainer->populateDesktop(true);
            ui->stackedWidget->setCurrentIndex(0);
            toggleWidgets(false);

            ui->worlds_menu->setStyleSheet(selectedMenu);
            ui->actionClose->setDisabled(true);

            if (UiManager::isSceneOpen) {
                ui->editor_menu->setStyleSheet(unselectedMenu);
                ui->editor_menu->setDisabled(false);
                ui->player_menu->setStyleSheet(unselectedMenu);
                ui->player_menu->setDisabled(false);
            } else {
                ui->editor_menu->setStyleSheet(disabledMenu);
                ui->editor_menu->setDisabled(true);
                ui->editor_menu->setCursor(Qt::ArrowCursor);
                ui->player_menu->setStyleSheet(disabledMenu);
                ui->player_menu->setDisabled(true);
                ui->player_menu->setCursor(Qt::ArrowCursor);
            }
            break;
        }

        case WindowSpaces::EDITOR: {
            ui->stackedWidget->setCurrentIndex(1);
            toggleWidgets(true);
            toolBar->setVisible(true);
            ui->worlds_menu->setStyleSheet(unselectedMenu);
            ui->editor_menu->setStyleSheet(selectedMenu);
            ui->editor_menu->setDisabled(false);
            ui->editor_menu->setCursor(Qt::PointingHandCursor);
            ui->player_menu->setStyleSheet(unselectedMenu);
            ui->player_menu->setDisabled(false);
            ui->player_menu->setCursor(Qt::PointingHandCursor);

            playSceneBtn->show();
            break;
        }

        case WindowSpaces::PLAYER: {
            ui->stackedWidget->setCurrentIndex(1);
            toggleWidgets(false);
            toolBar->setVisible(false);
            ui->worlds_menu->setStyleSheet(unselectedMenu);
            ui->editor_menu->setStyleSheet(unselectedMenu);
            ui->editor_menu->setDisabled(false);
            ui->editor_menu->setCursor(Qt::PointingHandCursor);
            ui->player_menu->setStyleSheet(selectedMenu);
            ui->player_menu->setDisabled(false);
            ui->player_menu->setCursor(Qt::PointingHandCursor);

            playSceneBtn->hide();
            break;
        }
        default: break;
    }
}

void MainWindow::saveScene()
{
    auto writer = new SceneWriter();
    auto blob = writer->getSceneObject(Globals::project->getProjectFolder(),
                                       scene,
                                       sceneView->getRenderer()->getPostProcessManager(),
                                       sceneView->getEditorData());

    auto img = sceneView->takeScreenshot(Constants::TILE_SIZE.width(), Constants::TILE_SIZE.height());
    QByteArray thumb;
    QBuffer buffer(&thumb);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    db->updateSceneGlobal(blob, thumb);
}

void MainWindow::openProject(bool playMode)
{
    this->sceneView->makeCurrent();

    // TODO - actually remove scenes - empty asset list, db cache and invalidate scene object
    this->removeScene();

    auto reader = new SceneReader();

    EditorData* editorData = nullptr;
    UiManager::updateWindowTitle();

    auto postMan = sceneView->getRenderer()->getPostProcessManager();
    postMan->clearPostProcesses();

    auto scene = reader->readScene(Globals::project->getProjectFolder(),
                                   db->getSceneBlobGlobal(),
                                   postMan,
                                   &editorData);

    UiManager::playMode = playMode;
    UiManager::isSceneOpen = true;
    ui->actionClose->setDisabled(false);
    setScene(scene);

    // use new post process that has fxaa by default
    // TODO: remember to find a better replacement (Nick)
    postProcessWidget->setPostProcessMgr(postMan);
    this->sceneView->doneCurrent();

    if (editorData != nullptr) {
        sceneView->setEditorData(editorData);
        wireCheckBtn->setChecked(editorData->showLightWires);
    }

    assetWidget->trigger();

    delete reader;

    // autoplay scenes immediately
    if (playMode) {
        playBtn->setToolTip("Pause the scene");
        playBtn->setIcon(QIcon(":/icons/g_pause.svg"));
        onPlaySceneButton();
    }

    UiManager::playMode ? switchSpace(WindowSpaces::PLAYER) : switchSpace(WindowSpaces::EDITOR);
}

void MainWindow::closeProject()
{
    UiManager::isSceneOpen = false;
    UiManager::isScenePlaying = false;
    ui->actionClose->setDisabled(false);

    switchSpace(WindowSpaces::DESKTOP);

    UiManager::clearUndoStack();
    AssetManager::assets.clear();

    scene->cleanup();
    scene.clear();
}

/// TODO - this needs to be fixed after the objects are added back to the uniforms array/obj
void MainWindow::applyMaterialPreset(MaterialPreset *preset)
{
    if (!activeSceneNode || activeSceneNode->sceneNodeType!=iris::SceneNodeType::Mesh) return;

    auto meshNode = activeSceneNode.staticCast<iris::MeshNode>();

    // TODO - set the TYPE for a preset in the .material file so we can have other preset types
    // only works for the default material at the moment...
    auto m = iris::CustomMaterial::create();
    m->generate(getAbsoluteAssetPath(Constants::DEFAULT_SHADER));

    m->setValue("diffuseTexture", preset->diffuseTexture);
    m->setValue("specularTexture", preset->specularTexture);
    m->setValue("normalTexture", preset->normalTexture);
    m->setValue("reflectionTexture", preset->reflectionTexture);

    m->setValue("ambientColor", preset->ambientColor);
    m->setValue("diffuseColor", preset->diffuseColor);
    m->setValue("specularColor", preset->specularColor);

    m->setValue("shininess", preset->shininess);
    m->setValue("normalIntensity", preset->normalIntensity);
    m->setValue("reflectionInfluence", preset->reflectionInfluence);
    m->setValue("textureScale", preset->textureScale);

    meshNode->setMaterial(m);

    // TODO: update node's material without updating the whole ui
    this->sceneNodePropertiesWidget->refreshMaterial(preset->type);
}

void MainWindow::setScene(QSharedPointer<iris::Scene> scene)
{
    this->scene = scene;
    this->sceneView->setScene(scene);
    this->sceneHierarchyWidget->setScene(scene);

    // interim...
    updateSceneSettings();
}

void MainWindow::removeScene()
{
}

void MainWindow::setupPropertyUi()
{
    animWidget = new AnimationWidget();
}

void MainWindow::sceneNodeSelected(QTreeWidgetItem* item)
{

}

void MainWindow::sceneTreeItemChanged(QTreeWidgetItem* item,int column)
{

}

void MainWindow::sceneNodeSelected(iris::SceneNodePtr sceneNode)
{
    activeSceneNode = sceneNode;

    sceneView->setSelectedNode(sceneNode);
    this->sceneNodePropertiesWidget->setSceneNode(sceneNode);
    this->sceneHierarchyWidget->setSelectedNode(sceneNode);
    animationWidget->setSceneNode(sceneNode);
}

void MainWindow::updateAnim()
{
}

void MainWindow::setSceneAnimTime(float time)
{
}

void MainWindow::addPlane()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/content/primitives/plane.obj");
    node->setFaceCullingMode(iris::FaceCullingMode::None);
    node->setName("Plane");
    addNodeToScene(node);
}

void MainWindow::addGround()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/models/ground.obj");
    node->setFaceCullingMode(iris::FaceCullingMode::None);
    node->setName("Ground");
    addNodeToScene(node);
}

void MainWindow::addCone()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/content/primitives/cone.obj");
    node->setName("Cone");
    addNodeToScene(node);
}

void MainWindow::addCube()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/content/primitives/cube.obj");
    node->setName("Cube");
    addNodeToScene(node);
}

void MainWindow::addTorus()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/content/primitives/torus.obj");
    node->setName("Torus");
    addNodeToScene(node);
}

void MainWindow::addSphere()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/content/primitives/sphere.obj");
    node->setName("Sphere");
    addNodeToScene(node);
}

void MainWindow::addCylinder()
{
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->setMesh(":/content/primitives/cylinder.obj");
    node->setName("Cylinder");

    addNodeToScene(node);
}

void MainWindow::addPointLight()
{
    this->sceneView->makeCurrent();
    auto node = iris::LightNode::create();
    node->setLightType(iris::LightType::Point);
    node->icon = iris::Texture2D::load(":/icons/bulb.png");
    node->setName("Point Light");
    node->intensity = 1.0f;
    node->distance = 40.0f;
    addNodeToScene(node);
}

void MainWindow::addSpotLight()
{
    this->sceneView->makeCurrent();
    auto node = iris::LightNode::create();
    node->setLightType(iris::LightType::Spot);
    node->icon = iris::Texture2D::load(":/icons/bulb.png");
    node->setName("Spot Light");
    addNodeToScene(node);
}


void MainWindow::addDirectionalLight()
{
    this->sceneView->makeCurrent();
    auto node = iris::LightNode::create();
    node->setLightType(iris::LightType::Directional);
    node->icon = iris::Texture2D::load(":/icons/bulb.png");
    node->setName("Directional Light");
    addNodeToScene(node);
}

void MainWindow::addEmpty()
{
    this->sceneView->makeCurrent();
    auto node = iris::SceneNode::create();
    node->setName("Empty");
    addNodeToScene(node);
}

void MainWindow::addViewer()
{
    this->sceneView->makeCurrent();
    auto node = iris::ViewerNode::create();
    node->setName("Viewer");
    addNodeToScene(node);
}

void MainWindow::addParticleSystem()
{
    this->sceneView->makeCurrent();
    auto node = iris::ParticleSystemNode::create();
    node->setName("Particle System");
    addNodeToScene(node);
}

void MainWindow::addMesh(const QString &path, bool ignore, QVector3D position)
{
    QString filename;
    if (path.isEmpty()) {
        filename = QFileDialog::getOpenFileName(this, "Load Mesh", "Mesh Files (*.obj *.fbx *.3ds *.dae *.c4d *.blend)");
    } else {
        filename = path;
    }

    if (filename.isEmpty()) return;

    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::loadAsSceneFragment(filename, [](iris::MeshPtr mesh, iris::MeshMaterialData& data)
    {
        auto mat = iris::CustomMaterial::create();
        //MaterialReader *materialReader = new MaterialReader();
        if (mesh->hasSkeleton())
            mat->generate(IrisUtils::getAbsoluteAssetPath("app/shader_defs/DefaultAnimated.shader"));
        else
            mat->generate(IrisUtils::getAbsoluteAssetPath("app/shader_defs/Default.shader"));

        mat->setValue("diffuseColor", data.diffuseColor);
        mat->setValue("specularColor", data.specularColor);
        mat->setValue("ambientColor", data.ambientColor);
        mat->setValue("emissionColor", data.emissionColor);

        mat->setValue("shininess", data.shininess);

        if (QFile(data.diffuseTexture).exists() && QFileInfo(data.diffuseTexture).isFile())
            mat->setValue("diffuseTexture", data.diffuseTexture);

        if (QFile(data.specularTexture).exists() && QFileInfo(data.specularTexture).isFile())
            mat->setValue("specularTexture", data.specularTexture);

        if (QFile(data.normalTexture).exists() && QFileInfo(data.normalTexture).isFile())
            mat->setValue("normalTexture", data.normalTexture);

        return mat;
    });

    // model file may be invalid so null gets returned
    if (!node) return;

    // rename animation sources to relative paths
    auto relPath = QDir(Globals::project->folderPath).relativeFilePath(filename);
    for (auto anim : node->getAnimations()) {
        if (!!anim->skeletalAnimation)
            anim->skeletalAnimation->source = relPath;
    }

    node->setLocalPos(position);

    // todo: load material data
    addNodeToScene(node, ignore);
}

void MainWindow::addDragPlaceholder()
{
    /*
    this->sceneView->makeCurrent();
    auto node = iris::MeshNode::create();
    node->scale = QVector3D(.5f, .5f, .5f);
    node->setMesh(":app/content/primitives/arrow.obj");
    node->setName("Arrow");
    addNodeToScene(node, true);
    */
}

/**
 * Adds sceneNode to selected scene node. If there is no selected scene node,
 * sceneNode is added to the root node
 * @param sceneNode
 */
void MainWindow::addNodeToActiveNode(QSharedPointer<iris::SceneNode> sceneNode)
{
    if (!scene) {
        //todo: set alert that a scene needs to be set before this can be done
    }

    // apply default material
    if (sceneNode->sceneNodeType == iris::SceneNodeType::Mesh) {
        auto meshNode = sceneNode.staticCast<iris::MeshNode>();

        if (!meshNode->getMaterial()) {
            auto mat = iris::DefaultMaterial::create();
            meshNode->setMaterial(mat);
        }
    }

    if (!!activeSceneNode) {
        activeSceneNode->addChild(sceneNode);
    } else {
        scene->getRootNode()->addChild(sceneNode);
    }

    this->sceneHierarchyWidget->repopulateTree();
}

/**
 * adds sceneNode directly to the scene's rootNode
 * applied default material to mesh if one isnt present
 * ignore set to false means we only add it visually, usually to discard it afterw
 */
void MainWindow::addNodeToScene(QSharedPointer<iris::SceneNode> sceneNode, bool ignore)
{
    if (!scene) {
        // @TODO: set alert that a scene needs to be set before this can be done
        return;
    }

    // @TODO: add this to a constants file
    if (!ignore) {
        const float spawnDist = 10.0f;
        auto offset = sceneView->editorCam->getLocalRot().rotatedVector(QVector3D(0, -1.0f, -spawnDist));
        offset += sceneView->editorCam->getLocalPos();
        sceneNode->setLocalPos(offset);
    }

    // apply default material to mesh nodes
    if (sceneNode->sceneNodeType == iris::SceneNodeType::Mesh) {
        auto meshNode = sceneNode.staticCast<iris::MeshNode>();
        if (!meshNode->getMaterial()) {
            auto mat = iris::CustomMaterial::create();
            mat->generate(IrisUtils::getAbsoluteAssetPath(Constants::DEFAULT_SHADER));
            meshNode->setMaterial(mat);
        }
    }

    auto cmd = new AddSceneNodeCommand(scene->getRootNode(), sceneNode);
    UiManager::pushUndoStack(cmd);
}

void MainWindow::repopulateSceneTree()
{
    this->sceneHierarchyWidget->repopulateTree();
}

void MainWindow::duplicateNode()
{
    if (!scene) return;
    if (!activeSceneNode || !activeSceneNode->isDuplicable()) return;

    auto node = activeSceneNode->duplicate();
    activeSceneNode->parent->addChild(node, false);

    this->sceneHierarchyWidget->repopulateTree();
    sceneNodeSelected(node);
}

void MainWindow::deleteNode()
{
    if (!!activeSceneNode) {
        auto cmd = new DeleteSceneNodeCommand(activeSceneNode->parent, activeSceneNode);
        UiManager::pushUndoStack(cmd);
    }
}


void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

/**
 * @brief accepts model files dropped into scene
 * currently only .obj files are supported
 */
void MainWindow::dropEvent(QDropEvent* event)
{

}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

/*
bool MainWindow::isModelExtension(QString extension)
{
    if(extension == "obj"   ||
       extension == "3ds"   ||
       extension == "fbx"   ||
       extension == "dae"   ||
       extension == "blend" ||
       extension == "c4d"   )
        return true;
    return false;
}
*/
void MainWindow::exportSceneAsZip()
{
    // get the export file path from a save dialog
    auto filePath = QFileDialog::getSaveFileName(
                        this,
                        "Choose export path",
                        Globals::project->getProjectName() + "_" + Constants::DEF_EXPORT_FILE,
                        "Supported Export Formats (*.zip)"
                    );

    if (filePath.isEmpty() || filePath.isNull()) return;

    if (!!scene) {
        saveScene();
    }

    // Maybe in the future one could add a way to using an in memory database
    // and saving saving that as a blob which can be put into the zip as bytes (iKlsR)
    // prepare our export database with the current scene, use the os temp location and remove after
    db->createExportScene(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    // get the current project working directory
    auto pFldr = IrisUtils::join(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                 Constants::PROJECT_FOLDER);
    auto defaultProjectDirectory = settings->getValue("default_directory", pFldr).toString();
    auto pDir = IrisUtils::join(defaultProjectDirectory, Globals::project->getProjectName());

    // get all the files and directories in the project working directory
    QDir workingProjectDirectory(pDir);
    QDirIterator projectDirIterator(pDir,
                                    QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs,
                                    QDirIterator::Subdirectories);

    QVector<QString> fileNames;
    while (projectDirIterator.hasNext()) fileNames.push_back(projectDirIterator.next());

    // open a basic zip file for writing, maybe change compression level later (iKlsR)
    struct zip_t *zip = zip_open(filePath.toStdString().c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');

    for (int i = 0; i < fileNames.count(); i++) {
        QFileInfo fInfo(fileNames[i]);

        // we need to pay special attention to directories since we want to write empty ones as well
        if (fInfo.isDir()) {
            zip_entry_open(
                zip,
                /* will only create directory if / is appended */
                QString(workingProjectDirectory.relativeFilePath(fileNames[i]) + "/").toStdString().c_str()
            );
            zip_entry_fwrite(zip, fileNames[i].toStdString().c_str());
        }
        else {
            zip_entry_open(
                zip,
                workingProjectDirectory.relativeFilePath(fileNames[i]).toStdString().c_str()
            );
            zip_entry_fwrite(zip, fileNames[i].toStdString().c_str());
        }

        // we close each entry after a successful write
        zip_entry_close(zip);
    }

    // finally add our exported scene
    zip_entry_open(zip, QString(Globals::project->getProjectName() + ".db").toStdString().c_str());
    zip_entry_fwrite(
        zip,
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(Globals::project->getProjectName() + ".db").toStdString().c_str()
    );
    zip_entry_close(zip);

    // close our now exported file
    zip_close(zip);

    // remove the temporary db created
    QDir tempFile;
    tempFile.remove(
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(Globals::project->getProjectName() + ".db")
                );
}

void MainWindow::setupDockWidgets()
{
    // Hierarchy Dock
    sceneHierarchyDock = new QDockWidget("Hierarchy", viewPort);
    sceneHierarchyDock->setObjectName(QStringLiteral("sceneHierarchyDock"));
    sceneHierarchyWidget = new SceneHierarchyWidget;
    sceneHierarchyWidget->setMinimumWidth(396);
    sceneHierarchyDock->setObjectName(QStringLiteral("sceneHierarchyWidget"));
    sceneHierarchyDock->setWidget(sceneHierarchyWidget);
    sceneHierarchyWidget->setMainWindow(this);

    UiManager::sceneHierarchyWidget = sceneHierarchyWidget;

    connect(sceneHierarchyWidget,   SIGNAL(sceneNodeSelected(iris::SceneNodePtr)),
            this,                   SLOT(sceneNodeSelected(iris::SceneNodePtr)));

    // Scene Node Properties Dock
    // Since this widget can be longer than there is screen space, we need to add a QScrollArea
    // For this to also work, we need a "holder widget" that will have a layout and the scroll area
    sceneNodePropertiesDock = new QDockWidget("Properties", viewPort);
    sceneNodePropertiesDock->setObjectName(QStringLiteral("sceneNodePropertiesDock"));
    sceneNodePropertiesWidget = new SceneNodePropertiesWidget;
    sceneNodePropertiesWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sceneNodePropertiesWidget->setObjectName(QStringLiteral("sceneNodePropertiesWidget"));

    QWidget *sceneNodeDockWidgetContents = new QWidget(viewPort);
    QScrollArea *sceneNodeScrollArea = new QScrollArea(sceneNodeDockWidgetContents);
    sceneNodeScrollArea->setMinimumWidth(396);
    sceneNodeScrollArea->setStyleSheet("border: 0");
    sceneNodeScrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    sceneNodeScrollArea->setWidget(sceneNodePropertiesWidget);
    sceneNodeScrollArea->setWidgetResizable(true);
    sceneNodeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QVBoxLayout *sceneNodeLayout = new QVBoxLayout(sceneNodeDockWidgetContents);
    sceneNodeLayout->setContentsMargins(0, 0, 0, 0);
    sceneNodeLayout->addWidget(sceneNodeScrollArea);
    sceneNodeDockWidgetContents->setLayout(sceneNodeLayout);
    sceneNodePropertiesDock->setWidget(sceneNodeDockWidgetContents);

    // Presets Dock
    presetsDock = new QDockWidget("Presets", viewPort);
    presetsDock->setObjectName(QStringLiteral("presetsDock"));

    QWidget *presetDockContents = new QWidget;
    MaterialSets *materialPresets = new MaterialSets;
    materialPresets->setMainWindow(this);
    SkyPresets *skyPresets = new SkyPresets;
    skyPresets->setMainWindow(this);
    ModelPresets *modelPresets = new ModelPresets;
    modelPresets->setMainWindow(this);

    presetsTabWidget = new QTabWidget;
    presetsTabWidget->setMinimumWidth(396);
    presetsTabWidget->addTab(modelPresets, "Primitives");
    presetsTabWidget->addTab(materialPresets, "Materials");
    presetsTabWidget->addTab(skyPresets, "Skyboxes");
    presetDockContents->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    QGridLayout *presetsLayout = new QGridLayout(presetDockContents);
    presetsLayout->setContentsMargins(0, 0, 0, 0);
    presetsLayout->addWidget(presetsTabWidget);
    presetsDock->setWidget(presetDockContents);

    // Asset Dock
    assetDock = new QDockWidget("Asset Browser", viewPort);
    assetDock->setObjectName(QStringLiteral("assetDock"));
    assetWidget = new AssetWidget(db, viewPort);
    assetWidget->setAcceptDrops(true);
    assetWidget->installEventFilter(this);

    QWidget *assetDockContents = new QWidget(viewPort);
    QGridLayout *assetsLayout = new QGridLayout(assetDockContents);
    assetsLayout->addWidget(assetWidget);
    assetsLayout->setContentsMargins(0, 0, 0, 0);
    assetDock->setWidget(assetDockContents);

    // Animation Dock
    animationDock = new QDockWidget("Timeline", viewPort);
    animationDock->setObjectName(QStringLiteral("animationDock"));
    animationWidget = new AnimationWidget;
    UiManager::setAnimationWidget(animationWidget);

    QWidget *animationDockContents = new QWidget;
    QGridLayout *animationLayout = new QGridLayout(animationDockContents);
    animationLayout->setContentsMargins(0, 0, 0, 0);
    animationLayout->addWidget(animationWidget);

    animationDock->setWidget(animationDockContents);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateAnim()));

    viewPort->addDockWidget(Qt::LeftDockWidgetArea, sceneHierarchyDock);
    viewPort->addDockWidget(Qt::RightDockWidgetArea, sceneNodePropertiesDock);
    viewPort->addDockWidget(Qt::BottomDockWidgetArea, assetDock);
    viewPort->addDockWidget(Qt::BottomDockWidgetArea, animationDock);
    viewPort->addDockWidget(Qt::BottomDockWidgetArea, presetsDock);
    viewPort->tabifyDockWidget(animationDock, assetDock);
}

void MainWindow::setupViewPort()
{
    connect(ui->worlds_menu, &QPushButton::pressed, [this]() { switchSpace(WindowSpaces::DESKTOP); });
    connect(ui->player_menu, &QPushButton::pressed, [this]() { switchSpace(WindowSpaces::PLAYER); });
    connect(ui->editor_menu, &QPushButton::pressed, [this]() { switchSpace(WindowSpaces::EDITOR); });

    sceneContainer = new QWidget;
    QSizePolicy sceneContainerPolicy;
    sceneContainerPolicy.setHorizontalPolicy(QSizePolicy::Preferred);
    sceneContainerPolicy.setVerticalPolicy(QSizePolicy::Preferred);
    sceneContainerPolicy.setVerticalStretch(1);
    sceneContainer->setSizePolicy(sceneContainerPolicy);
    sceneContainer->setAcceptDrops(true);
    sceneContainer->installEventFilter(this);

    controlBar = new QWidget;
    controlBar->setObjectName(QStringLiteral("controlBar"));

    auto container = new QWidget;
    auto containerLayout = new QVBoxLayout;

    auto screenShotBtn = new QPushButton;
    screenShotBtn->setToolTip("Take a screenshot of the scene");
    screenShotBtn->setToolTipDuration(-1);
    screenShotBtn->setStyleSheet("background: transparent");
    screenShotBtn->setIcon(QIcon(":/icons/camera.svg"));

    wireCheckBtn = new QCheckBox("Viewport Wireframes");
    wireCheckBtn->setCheckable(true);

    connect(screenShotBtn, SIGNAL(pressed()), this, SLOT(takeScreenshot()));
    connect(wireCheckBtn, SIGNAL(toggled(bool)), this, SLOT(toggleLightWires(bool)));

    auto controlBarLayout = new QHBoxLayout;
    playSceneBtn = new QPushButton;
    playSceneBtn->setToolTip("Play scene");
    playSceneBtn->setToolTipDuration(-1);
    playSceneBtn->setStyleSheet("background: transparent");
    playSceneBtn->setIcon(QIcon(":/icons/g_play.svg"));

    controlBarLayout->setSpacing(8);
    controlBarLayout->addWidget(screenShotBtn);
    controlBarLayout->addWidget(wireCheckBtn);
    controlBarLayout->addStretch();
    controlBarLayout->addWidget(playSceneBtn);

    controlBar->setLayout(controlBarLayout);
    controlBar->setStyleSheet("#controlBar {  background: #1A1A1A; border-bottom: 1px solid black; }");

    playerControls = new QWidget;
    playerControls->setStyleSheet("background: #1A1A1A");

    auto playerControlsLayout = new QHBoxLayout;

    restartBtn = new QPushButton;
    restartBtn->setCursor(Qt::PointingHandCursor);
    restartBtn->setToolTip("Restart playback");
    restartBtn->setToolTipDuration(-1);
    restartBtn->setStyleSheet("background: transparent");
    restartBtn->setIcon(QIcon(":/icons/rotate-to-right.svg"));
    restartBtn->setIconSize(QSize(16, 16));

    playBtn = new QPushButton;
    playBtn->setCursor(Qt::PointingHandCursor);
    playBtn->setToolTip("Play the scene");
    playBtn->setToolTipDuration(-1);
    playBtn->setStyleSheet("background: transparent");
    playBtn->setIcon(QIcon(":/icons/g_play.svg"));
    playBtn->setIconSize(QSize(24, 24));

    stopBtn = new QPushButton;
    stopBtn->setCursor(Qt::PointingHandCursor);
    stopBtn->setToolTip("Stop playback");
    stopBtn->setToolTipDuration(-1);
    stopBtn->setStyleSheet("background: transparent");
    stopBtn->setIcon(QIcon(":/icons/g_stop.svg"));
    stopBtn->setIconSize(QSize(16, 16));

    playerControlsLayout->setSpacing(12);
    playerControlsLayout->setMargin(6);
    playerControlsLayout->addStretch();
    playerControlsLayout->addWidget(restartBtn);
    playerControlsLayout->addWidget(playBtn);
    playerControlsLayout->addWidget(stopBtn);
    playerControlsLayout->addStretch();

    connect(restartBtn, &QPushButton::pressed, [this]() {
        playBtn->setToolTip("Pause the scene");
        playBtn->setIcon(QIcon(":/icons/g_pause.svg"));
        UiManager::restartScene();
    });

    connect(playBtn, &QPushButton::pressed, [this]() {
        if (UiManager::isScenePlaying) {
            playBtn->setToolTip("Play the scene");
            playBtn->setIcon(QIcon(":/icons/g_play.svg"));
            UiManager::pauseScene();
        } else {
            playBtn->setToolTip("Pause the scene");
            playBtn->setIcon(QIcon(":/icons/g_pause.svg"));
            UiManager::playScene();
        }
    });
    connect(stopBtn, &QPushButton::pressed, [this]() {
        playBtn->setToolTip("Play the scene");
        playBtn->setIcon(QIcon(":/icons/g_play.svg"));
        UiManager::stopScene();
    });

    playerControls->setLayout(playerControlsLayout);

    containerLayout->setSpacing(0);
    containerLayout->setMargin(0);
    containerLayout->addWidget(controlBar);
    containerLayout->addWidget(sceneContainer);
    containerLayout->addWidget(playerControls);

    container->setLayout(containerLayout);

    viewPort = new QMainWindow;
    viewPort->setWindowFlags(Qt::Widget);
    viewPort->setCentralWidget(container);

    sceneView = new SceneViewWidget(viewPort);
    sceneView->setParent(viewPort);
    sceneView->setFocusPolicy(Qt::ClickFocus);
    sceneView->setFocus();
    Globals::sceneViewWidget = sceneView;
    UiManager::setSceneViewWidget(sceneView);

    wireCheckBtn->setChecked(sceneView->getShowLightWires());

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(sceneView);
    layout->setMargin(0);
    sceneContainer->setLayout(layout);

    connect(sceneView,  SIGNAL(initializeGraphics(SceneViewWidget*, QOpenGLFunctions_3_2_Core*)),
            this,       SLOT(initializeGraphics(SceneViewWidget*,   QOpenGLFunctions_3_2_Core*)));

    connect(sceneView,  SIGNAL(sceneNodeSelected(iris::SceneNodePtr)),
            this,       SLOT(sceneNodeSelected(iris::SceneNodePtr)));

    connect(playSceneBtn, SIGNAL(clicked(bool)), SLOT(onPlaySceneButton()));
}

void MainWindow::setupDesktop()
{
    pmContainer = new ProjectManager(db, this);

    ui->stackedWidget->addWidget(pmContainer);
    ui->stackedWidget->addWidget(viewPort);

    connect(pmContainer, SIGNAL(fileToOpen(bool)), SLOT(openProject(bool)));
    connect(pmContainer, SIGNAL(closeProject()), SLOT(closeProject()));
    connect(pmContainer, SIGNAL(fileToCreate(QString, QString)), SLOT(newProject(QString, QString)));
    connect(pmContainer, SIGNAL(exportProject()), SLOT(exportSceneAsZip()));
}

void MainWindow::setupToolBar()
{
    toolBar = new QToolBar;
    QAction *actionTranslate = new QAction;
    actionTranslate->setObjectName(QStringLiteral("actionTranslate"));
    actionTranslate->setCheckable(true);
    actionTranslate->setIcon(QIcon(":/icons/tranlate arrow.svg"));
    toolBar->addAction(actionTranslate);

    QAction *actionRotate = new QAction;
    actionRotate->setObjectName(QStringLiteral("actionRotate"));
    actionRotate->setCheckable(true);
    actionRotate->setIcon(QIcon(":/icons/rotate-to-right.svg"));
    toolBar->addAction(actionRotate);

    QAction *actionScale = new QAction;
    actionScale->setObjectName(QStringLiteral("actionScale"));
    actionScale->setCheckable(true);
    actionScale->setIcon(QIcon(":/icons/expand-arrows.svg"));
    toolBar->addAction(actionScale);

    toolBar->addSeparator();

    QAction *actionGlobalSpace = new QAction;
    actionGlobalSpace->setObjectName(QStringLiteral("actionGlobalSpace"));
    actionGlobalSpace->setCheckable(true);
    actionGlobalSpace->setIcon(QIcon(":/icons/world.svg"));
    toolBar->addAction(actionGlobalSpace);

    QAction *actionLocalSpace = new QAction;
    actionLocalSpace->setObjectName(QStringLiteral("actionLocalSpace"));
    actionLocalSpace->setCheckable(true);
    actionLocalSpace->setIcon(QIcon(":/icons/sceneobject.svg"));
    toolBar->addAction(actionLocalSpace);

    toolBar->addSeparator();

    QAction *actionFreeCamera = new QAction;
    actionFreeCamera->setObjectName(QStringLiteral("actionFreeCamera"));
    actionFreeCamera->setCheckable(true);
    actionFreeCamera->setIcon(QIcon(":/icons/people.svg"));
    toolBar->addAction(actionFreeCamera);

    QAction *actionArcballCam = new QAction;
    actionArcballCam->setObjectName(QStringLiteral("actionArcballCam"));
    actionArcballCam->setCheckable(true);
    actionArcballCam->setIcon(QIcon(":/icons/local.svg"));
    toolBar->addAction(actionArcballCam);

    connect(actionTranslate,    SIGNAL(triggered(bool)), SLOT(translateGizmo()));
    connect(actionRotate,       SIGNAL(triggered(bool)), SLOT(rotateGizmo()));
    connect(actionScale,        SIGNAL(triggered(bool)), SLOT(scaleGizmo()));

    transformGroup = new QActionGroup(viewPort);
    transformGroup->addAction(actionTranslate);
    transformGroup->addAction(actionRotate);
    transformGroup->addAction(actionScale);

    connect(actionGlobalSpace,  SIGNAL(triggered(bool)), SLOT(useGlobalTransform()));
    connect(actionLocalSpace,   SIGNAL(triggered(bool)), SLOT(useLocalTransform()));

    transformSpaceGroup = new QActionGroup(viewPort);
    transformSpaceGroup->addAction(actionGlobalSpace);
    transformSpaceGroup->addAction(actionLocalSpace);
    ui->actionGlobalSpace->setChecked(true);

    connect(actionFreeCamera,   SIGNAL(triggered(bool)), SLOT(useFreeCamera()));
    connect(actionArcballCam,   SIGNAL(triggered(bool)), SLOT(useArcballCam()));

    cameraGroup = new QActionGroup(viewPort);
    cameraGroup->addAction(actionFreeCamera);
    cameraGroup->addAction(actionArcballCam);
    actionFreeCamera->setChecked(true);

    // this acts as a spacer
    QWidget* empty = new QWidget();
    empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolBar->addWidget(empty);

    vrButton = new QPushButton();
    QIcon icovr(":/icons/virtual-reality.svg");
    vrButton->setIcon(icovr);
    vrButton->setObjectName("vrButton");
    toolBar->addWidget(vrButton);

    viewPort->addToolBar(toolBar);
}

void MainWindow::setupShortcuts()
{
    // Translation, Rotation and Scaling gizmo shortcuts for
    QShortcut *shortcut = new QShortcut(QKeySequence("t"),sceneView);
    connect(shortcut, SIGNAL(activated()), this, SLOT(translateGizmo()));

    shortcut = new QShortcut(QKeySequence("r"),sceneView);
    connect(shortcut, SIGNAL(activated()), this, SLOT(rotateGizmo()));

    shortcut = new QShortcut(QKeySequence("s"),sceneView);
    connect(shortcut, SIGNAL(activated()), this, SLOT(scaleGizmo()));

    // Save
    shortcut = new QShortcut(QKeySequence("ctrl+s"),sceneView);
    connect(shortcut, SIGNAL(activated()), this, SLOT(saveScene()));
}

QIcon MainWindow::getIconFromSceneNodeType(SceneNodeType type)
{
    return QIcon();
}

void MainWindow::showPreferences()
{
    prefsDialog->exec();
}

void MainWindow::exitApp()
{
    QApplication::exit();
}

void MainWindow::updateSceneSettings()
{
    scene->setOutlineWidth(prefsDialog->worldSettings->outlineWidth);
    scene->setOutlineColor(prefsDialog->worldSettings->outlineColor);
}

void MainWindow::undo()
{
    if (undoStack->canUndo()) undoStack->undo();
}

void MainWindow::redo()
{
    if (undoStack->canRedo()) undoStack->redo();
}

void MainWindow::takeScreenshot()
{
    auto img = sceneView->takeScreenshot();
    ScreenshotWidget screenshotWidget;
    screenshotWidget.setMaximumWidth(1280);
    screenshotWidget.setMaximumHeight(720);
    screenshotWidget.layout()->setSizeConstraint(QLayout::SetNoConstraint);
    screenshotWidget.setImage(img);
    screenshotWidget.exec();
}

void MainWindow::toggleLightWires(bool state)
{
    sceneView->setShowLightWires(state);
}

void MainWindow::toggleWidgets(bool state)
{
    sceneHierarchyDock->setVisible(state);
    sceneNodePropertiesDock->setVisible(state);
    presetsDock->setVisible(state);
    assetDock->setVisible(state);
    animationDock->setVisible(state);
    playerControls->setVisible(!state);
}

void MainWindow::showProjectManagerInternal()
{
    if (UiManager::isUndoStackDirty()) {
        QMessageBox::StandardButton option;
        option = QMessageBox::question(this,
                                       "Unsaved Changes",
                                       "There are unsaved changes, save before closing?",
                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (option == QMessageBox::Yes) {
            saveScene();
        } else if (option == QMessageBox::Cancel) {
            return;
        }
    }

    if (UiManager::isScenePlaying) enterEditMode();
    hide();
    pmContainer->populateDesktop(true);
    pmContainer->showMaximized();
    pmContainer->cleanupOnClose();
}

void MainWindow::newScene()
{
    this->showMaximized();

    this->sceneView->makeCurrent();
    auto scene = this->createDefaultScene();
    this->setScene(scene);
    this->sceneView->resetEditorCam();
    this->sceneView->doneCurrent();
}

void MainWindow::newProject(const QString &filename, const QString &projectPath)
{
    newScene();

    auto pPath = QDir(projectPath).filePath(filename + Constants::PROJ_EXT);

    auto writer = new SceneWriter();
    auto sceneObject = writer->getSceneObject(pPath,
                                              this->scene,
                                              sceneView->getRenderer()->getPostProcessManager(),
                                              sceneView->getEditorData());
    db->insertSceneGlobal(filename, sceneObject);

    UiManager::updateWindowTitle();

    assetWidget->trigger();

    UiManager::isSceneOpen = true;
    ui->actionClose->setDisabled(false);
    switchSpace(WindowSpaces::EDITOR);

    delete writer;
}

void MainWindow::showAboutDialog()
{
    aboutDialog->exec();
}

void MainWindow::showLicenseDialog()
{
    licenseDialog->exec();
}

void MainWindow::openFacebookUrl()
{
    QDesktopServices::openUrl(QUrl("https://www.facebook.com/jahshakafx/"));
}

void MainWindow::openWebsiteUrl()
{
    QDesktopServices::openUrl(QUrl("http://www.jahshaka.com/"));
}

MainWindow::~MainWindow()
{
    this->db->closeDb();
    delete ui;
}

void MainWindow::useFreeCamera()
{
    sceneView->setFreeCameraMode();
}

void MainWindow::useArcballCam()
{
    sceneView->setArcBallCameraMode();
}

void MainWindow::useLocalTransform()
{
    sceneView->setTransformOrientationLocal();
}

void MainWindow::useGlobalTransform()
{
    sceneView->setTransformOrientationGlobal();
}

void MainWindow::translateGizmo()
{
    sceneView->setGizmoLoc();
}

void MainWindow::rotateGizmo()
{
    sceneView->setGizmoRot();
}

void MainWindow::scaleGizmo()
{
    sceneView->setGizmoScale();
}

void MainWindow::onPlaySceneButton()
{
    if (UiManager::isScenePlaying) {
        enterEditMode();
    }
    else {
        enterPlayMode();
    }
}

void MainWindow::enterEditMode()
{
    UiManager::isScenePlaying = false;
    UiManager::enterEditMode();
    playSceneBtn->setToolTip("Play scene");
    playSceneBtn->setIcon(QIcon(":/icons/g_play.svg"));
}

void MainWindow::enterPlayMode()
{
    UiManager::isScenePlaying = true;
    UiManager::enterPlayMode();
    playSceneBtn->setToolTip("Stop playing");
    playSceneBtn->setIcon(QIcon(":/icons/g_stop.svg"));
}
