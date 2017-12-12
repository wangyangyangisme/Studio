#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QCryptographicHash>

#include "../../core/project.h"

class Database
{
public:
    Database();
    ~Database();

    void executeAndCheckQuery(QSqlQuery&, const QString&);
    void initializeDatabase(QString name);
    void closeDb();

    void createGlobalDb();
    void createGlobalDbThumbs();
	void createGlobalDbAssets();
    void deleteProject();
    void renameProject(const QString&);
	QString insertAssetGlobal(const QString&, const QByteArray &thumbnail);
    void insertSceneGlobal(const QString &world_guid, const QByteArray &sceneBlob);
    void insertThumbnailGlobal(const QString &world_guid,
                               const QString &name,
                               const QByteArray &thumbnail);
    bool hasCachedThumbnail(const QString& name);
    QVector<ProjectTileData> fetchProjects();
	QVector<AssetTileData> fetchAssets();
    QByteArray getSceneBlobGlobal() const;
    QByteArray fetchCachedThumbnail(const QString& name) const;
    void updateSceneGlobal(const QByteArray &sceneBlob, const QByteArray &thumbnail);
    void createExportScene(const QString& outTempFilePath);
    bool importProject(const QString& inFilePath);

    QSqlDatabase getDb() { return db; }

private:
    QSqlDatabase db;
};

#endif // DATABASE_H
