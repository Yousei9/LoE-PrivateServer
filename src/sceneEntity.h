#ifndef SCENEENTITY_H
#define SCENEENTITY_H

#include <QString>
#include <QMutex>
#include "dataType.h"

struct SceneEntity
{
public:
    SceneEntity();

public:
    // Infos
    QString modelName;
    quint16 id;
    quint16 netviewId;

    // Pos
    QString sceneName;
    UVector pos;
    UQuaternion rot;

public:
    static int getNewNetviewId();
    static int getNewId();

public:
    static int lastNetviewId;
    static int lastId;
    static QMutex lastIdMutex; // Protects lastId and lastNetviewId
    static bool usedids[];
};

#endif // SCENEENTITY_H
