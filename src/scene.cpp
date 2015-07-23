#include "scene.h"
#include "log.h"
#include <QFile>
#include <QtXml/qdom.h>

QList<Scene> Scene::scenes; // List of scenes from the vortex DB

Vortex::Vortex()
{
    id = 0;
    destName = QString();
    destPos = UVector();
    destRot = UQuaternion();
}

Scene::Scene(QString sceneName)
{
    name = sceneName.toLower();
    vortexes = QList<Vortex>();
}

bool ReadVortxXml(QString file)
{
    QDomDocument doc;
    QFile xmlfile(file);
    if(!xmlfile.open(QIODevice::ReadOnly) || !doc.setContent(&xmlfile))
    {
        logError(QObject::tr("Error reading %1").arg(file));
        return false;
    }

    QDomNodeList nodeListScene = doc.elementsByTagName("scene");

    if (nodeListScene.isEmpty() || nodeListScene.length() != 1)
    {
        logError(QObject::tr("Error parsing %1. sceneNodes size missmatch. 1 expected %2 read.").arg(file).arg(nodeListScene.length()));
        return false;
    }

    if (!nodeListScene.item(0).isElement())
    {
        logError(QObject::tr("Error parsing %1. Scene node couldn't be read as element.").arg(file));
        return false;
    }
    QDomElement nodeScene = nodeListScene.item(0).toElement();

    if (nodeScene.attribute("name").isNull() || nodeScene.attribute("name").isEmpty())
    {
        logError(QObject::tr("Error parsing %1. Scene name attribute must be set").arg(file));
        return false;
    }
    Scene scene(nodeScene.attribute("name"));

    QDomNodeList nodeListVortex = nodeListScene.item(0).childNodes();
    for (int i = 0; i < nodeListVortex.size(); i++)
    {
        QDomNode n = nodeListVortex.item(i);

        QDomElement nodeVortexId = n.firstChildElement("id");
        QDomElement nodeVortexDestName = n.firstChildElement("name");
        QDomElement nodeVortexDestPos = n.firstChildElement("pos");
        QDomElement nodeVortexDestRot = n.firstChildElement("rot");

        Vortex vortex;
        bool okId, okPosX, okPosY, okPosZ, okRotX, okRotY, okRotZ, okRotW;

        vortex.id = nodeVortexId.text().toUInt(&okId, 16);
        vortex.destName = nodeVortexDestName.text();
        vortex.destPos.x = nodeVortexDestPos.attribute("x").toFloat(&okPosX);
        vortex.destPos.y = nodeVortexDestPos.attribute("y").toFloat(&okPosY);
        vortex.destPos.z = nodeVortexDestPos.attribute("z").toFloat(&okPosZ);

        vortex.destRot.x = nodeVortexDestRot.attribute("x","0").toFloat(&okRotX);
        vortex.destRot.y = nodeVortexDestRot.attribute("y","0").toFloat(&okRotY);
        vortex.destRot.z = nodeVortexDestRot.attribute("z","0").toFloat(&okRotZ);
        vortex.destRot.w = nodeVortexDestRot.attribute("w","0").toFloat(&okRotW);

        if (okId && okPosX && okPosY && okPosZ && okRotX && okRotY && okRotZ && okRotW)
        {
           scene.vortexes << vortex;
        }
        else
        {
            logStatusError(QObject::tr("Error parsing %1. Can't convert Vortex data").arg(file));
            return false;
        }
    }

    Scene::scenes << scene;
    xmlfile.close();
    return true;
}

Scene* findScene(QString sceneName)
{
    for (int i=0; i<Scene::scenes.size(); i++)
        if (Scene::scenes[i].name.toLower() == sceneName.toLower())
            return &Scene::scenes[i];

    return new Scene("");
}

Vortex findVortex(QString sceneName, quint8 id)
{
    Scene scene(sceneName);
    for (int i=0; i<Scene::scenes.size(); i++)
        if (Scene::scenes[i].name .toLower()== sceneName.toLower())
            scene = Scene::scenes[i];

    for (int i=0; i<scene.vortexes.size(); i++)
        if (scene.vortexes[i].id == id)
            return scene.vortexes[i];

    return Vortex();
}

Vortex findVortex(Scene* scene, quint8 id)
{
    for (int i=0; i<scene->vortexes.size(); i++)
        if (scene->vortexes[i].id == id)
            return scene->vortexes[i];

    return Vortex();
}
