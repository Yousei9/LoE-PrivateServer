#ifndef SCENE_H
#define SCENE_H

#include <QString>
#include <QList>
#include "dataType.h"

class Player;

class Vortex
{
public:
    Vortex();

public:
    quint8 id;
    QString destName;
    UVector destPos;
    UQuaternion destRot;
};

class Scene
{
public:
    Scene(QString sceneName);

public:
    QString name;
    QList<Vortex> vortexes;
    QList<Player*> players; // Used by the 01 sync function

public:
    static QList<Scene> scenes; // List of scenes from the vortex DB
};
Scene* findScene(QString sceneName);
Vortex findVortex(QString sceneName, quint8 id);
Vortex findVortex(Scene* scene, quint8 id);
bool ReadVortxXml(QString file);

#endif // SCENE_H
