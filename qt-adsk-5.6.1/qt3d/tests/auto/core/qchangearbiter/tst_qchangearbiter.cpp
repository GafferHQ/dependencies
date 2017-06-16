/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QTest>
#include <Qt3DCore/private/qobserverinterface_p.h>
#include <Qt3DCore/private/qobservableinterface_p.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DCore/private/qpostman_p.h>
#include <Qt3DCore/qscenepropertychange.h>
#include <Qt3DCore/qscenechange.h>
#include <Qt3DCore/qbackendscenepropertychange.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qnode.h>
#include <Qt3DCore/qbackendnode.h>
#include <Qt3DCore/private/qsceneobserverinterface_p.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include <QThread>
#include <QWaitCondition>

class tst_QChangeArbiter : public QObject
{
    Q_OBJECT

private slots:
    void registerObservers();
    void registerSceneObserver();
    void unregisterObservers();
    void unregisterSceneObservers();
    void distributeFrontendChanges();
    void distributeBackendChanges();
};

class tst_Node : public Qt3DCore::QNode
{
public:
    explicit tst_Node(Qt3DCore::QNode *parent = 0) : Qt3DCore::QNode(parent)
    {}

    void sendNodeAddedNotification()
    {
        Qt3DCore::QScenePropertyChangePtr e(new Qt3DCore::QScenePropertyChange(Qt3DCore::NodeAdded, Qt3DCore::QSceneChange::Node, id()));
        e->setPropertyName("NodeAdded");
        Qt3DCore::QNodePrivate::get(this)->notifyObservers(e);
    }

    void sendNodeRemovedNotification()
    {
        Qt3DCore::QScenePropertyChangePtr e(new Qt3DCore::QScenePropertyChange(Qt3DCore::NodeRemoved, Qt3DCore::QSceneChange::Node, id()));
        e->setPropertyName("NodeRemoved");
        Qt3DCore::QNodePrivate::get(this)->notifyObservers(e);
    }

    void sendNodeUpdatedNotification()
    {
        Qt3DCore::QScenePropertyChangePtr e(new Qt3DCore::QScenePropertyChange(Qt3DCore::NodeUpdated, Qt3DCore::QSceneChange::Node, id()));
        e->setPropertyName("NodeUpdated");
        Qt3DCore::QNodePrivate::get(this)->notifyObservers(e);
    }

    void sendComponentAddedNotification()
    {
        Qt3DCore::QScenePropertyChangePtr e(new Qt3DCore::QScenePropertyChange(Qt3DCore::ComponentAdded, Qt3DCore::QSceneChange::Node, id()));
        e->setPropertyName("ComponentAdded");
        Qt3DCore::QNodePrivate::get(this)->notifyObservers(e);
    }

    void sendComponentRemovedNotification()
    {
        Qt3DCore::QScenePropertyChangePtr e(new Qt3DCore::QScenePropertyChange(Qt3DCore::ComponentRemoved, Qt3DCore::QSceneChange::Node, id()));
        e->setPropertyName("ComponentRemoved");
        Qt3DCore::QNodePrivate::get(this)->notifyObservers(e);
    }

    void sendAllChangesNotification()
    {
        Qt3DCore::QScenePropertyChangePtr e(new Qt3DCore::QScenePropertyChange(Qt3DCore::AllChanges, Qt3DCore::QSceneChange::Node, id()));
        e->setPropertyName("AllChanges");
        Qt3DCore::QNodePrivate::get(this)->notifyObservers(e);
    }

    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change) Q_DECL_OVERRIDE
    {
        QVERIFY(!change.isNull());
        m_lastChanges << change;
    }

    Qt3DCore::QSceneChangePtr lastChange() const
    {
        if (m_lastChanges.isEmpty())
            return Qt3DCore::QSceneChangePtr();
        return m_lastChanges.last();
    }

    QList<Qt3DCore::QSceneChangePtr> lastChanges() const
    {
        return m_lastChanges;
    }

    // QNode interface
protected:
    Qt3DCore::QNode *doClone() const Q_DECL_OVERRIDE
    {
        tst_Node *clone = new tst_Node();
        clone->copy(this);
        return clone;
    }

private:
    QList<Qt3DCore::QSceneChangePtr> m_lastChanges;
};

class tst_SimpleObserver : public Qt3DCore::QObserverInterface
{
public:

    // QObserverInterface interface
    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e) Q_DECL_OVERRIDE
    {
        QVERIFY(!e.isNull());
        m_lastChanges.append(e);
    }

    Qt3DCore::QSceneChangePtr lastChange() const
    {
        if (m_lastChanges.isEmpty())
            return Qt3DCore::QSceneChangePtr();
        return m_lastChanges.last();
    }

    QList<Qt3DCore::QSceneChangePtr> lastChanges() const
    {
        return m_lastChanges;
    }

private:
    QList<Qt3DCore::QSceneChangePtr> m_lastChanges;
};

class tst_BackendObserverObservable : public Qt3DCore::QBackendNode
{
public:

    tst_BackendObserverObservable()
        : Qt3DCore::QBackendNode(ReadWrite)
    {}

    void updateFromPeer(Qt3DCore::QNode *) Q_DECL_OVERRIDE
    {}

    // QObserverInterface interface
    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e) Q_DECL_OVERRIDE
    {
        QVERIFY(!e.isNull());
        m_lastChanges << e;
        // Save reply to be sent to the frontend
        m_reply.reset(new Qt3DCore::QBackendScenePropertyChange(Qt3DCore::NodeUpdated, e->subjectId()));
        m_reply->setTargetNode(e->subjectId());
        m_reply->setPropertyName("Reply");
    }

    // should be called in thread
    void sendReply()
    {
        QVERIFY(!m_reply.isNull());
        notifyObservers(m_reply);
        qDebug() << Q_FUNC_INFO;
    }

    Qt3DCore::QSceneChangePtr lastChange() const
    {
        if (m_lastChanges.isEmpty())
            return Qt3DCore::QSceneChangePtr();
        return m_lastChanges.last();
    }

    QList<Qt3DCore::QSceneChangePtr> lastChanges() const
    {
        return m_lastChanges;
    }

private:
    QList<Qt3DCore::QSceneChangePtr> m_lastChanges;
    Qt3DCore::QBackendScenePropertyChangePtr m_reply;

};

QWaitCondition waitingForBackendReplyCondition;

class ThreadedAnswer : public QThread
{
    Q_OBJECT
public:
    ThreadedAnswer(Qt3DCore::QChangeArbiter *arbiter, tst_BackendObserverObservable *backend)
        : QThread()
        , m_arbiter(arbiter)
        , m_backendObs(backend)
    {}

    void run() Q_DECL_OVERRIDE
    {
        // create backend change queue on QChangeArbiter
        Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(m_arbiter);
        m_backendObs->sendReply();
        // gives time for other threads to start waiting
        QThread::currentThread()->sleep(1);
        // wake waiting condition
        waitingForBackendReplyCondition.wakeOne();
        exec();
    }

private:
    Qt3DCore::QChangeArbiter *m_arbiter;
    tst_BackendObserverObservable *m_backendObs;
    QWaitCondition m_waitingForReplyToBeSent;
};

class tst_PostManObserver : public Qt3DCore::QAbstractPostman
{
public:

    tst_PostManObserver() : m_sceneInterface(Q_NULLPTR)
    {}

    void setScene(Qt3DCore::QScene *scene) Q_DECL_FINAL
    {
        m_sceneInterface = scene;
    }

    // QObserverInterface interface
    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
    {
        QVERIFY(!e.isNull());
        Qt3DCore::QBackendScenePropertyChangePtr change = qSharedPointerDynamicCast<Qt3DCore::QBackendScenePropertyChange>(e);
        QVERIFY(!change.isNull());
        Qt3DCore::QNode *targetNode = m_sceneInterface->lookupNode(change->targetNode());
        QVERIFY(targetNode != Q_NULLPTR);
        m_lastChanges << e;
    }

    Qt3DCore::QSceneChangePtr lastChange() const
    {
        if (m_lastChanges.isEmpty())
            return Qt3DCore::QSceneChangePtr();
        return m_lastChanges.last();
    }

    QList<Qt3DCore::QSceneChangePtr> lastChanges() const
    {
        return m_lastChanges;
    }

    void notifyBackend(const Qt3DCore::QSceneChangePtr &e) Q_DECL_FINAL
    {
        m_sceneInterface->arbiter()->sceneChangeEventWithLock(e);
    }

private:
    Qt3DCore::QScene *m_sceneInterface;
    QList<Qt3DCore::QSceneChangePtr> m_lastChanges;
};

class tst_SceneObserver : public Qt3DCore::QSceneObserverInterface
{
    // QSceneObserverInterface interface
public:
    void sceneNodeAdded(Qt3DCore::QSceneChangePtr &e)
    {
        QVERIFY(!e.isNull());
        QVERIFY(e->type() == Qt3DCore::NodeCreated);
        m_lastChange = e;
    }

    void sceneNodeRemoved(Qt3DCore::QSceneChangePtr &e)
    {
        QVERIFY(!e.isNull());
        QVERIFY((e->type() == Qt3DCore::NodeDeleted || e->type() == Qt3DCore::NodeAboutToBeDeleted));
        m_lastChange = e;
    }

    void sceneNodeUpdated(Qt3DCore::QSceneChangePtr &e)
    {
        m_lastChange = e;
    }

    Qt3DCore::QSceneChangePtr lastChange() const
    {
        return m_lastChange;
    }

private:
    Qt3DCore::QSceneChangePtr m_lastChange;
};

void tst_QChangeArbiter::registerObservers()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QChangeArbiter> arbiter(new Qt3DCore::QChangeArbiter());
    QScopedPointer<Qt3DCore::QScene> scene(new Qt3DCore::QScene());
    QScopedPointer<Qt3DCore::QAbstractPostman> postman(new tst_PostManObserver);
    arbiter->setPostman(postman.data());
    arbiter->setScene(scene.data());
    postman->setScene(scene.data());
    scene->setArbiter(arbiter.data());
    // Replaces initialize as we have no JobManager in this case
    Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(arbiter.data());

    // WHEN
    Qt3DCore::QNode *root = new tst_Node();
    Qt3DCore::QNode *child = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene.data());
    scene->addObservable(root);

    QList<tst_SimpleObserver *> observers;
    for (int i = 0; i < 5; i++) {
        tst_SimpleObserver *s = new tst_SimpleObserver();
        arbiter->registerObserver(s, root->id());
        observers << s;
    }

    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers)
        QVERIFY(o->lastChange().isNull());

    child->setParent(root);
    arbiter->syncChanges();
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    Qt3DCore::QChangeArbiter::destroyThreadLocalChangeQueue(arbiter.data());
}

void tst_QChangeArbiter::registerSceneObserver()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QChangeArbiter> arbiter(new Qt3DCore::QChangeArbiter());
    QScopedPointer<Qt3DCore::QScene> scene(new Qt3DCore::QScene());
    QScopedPointer<Qt3DCore::QAbstractPostman> postman(new tst_PostManObserver);
    arbiter->setPostman(postman.data());
    arbiter->setScene(scene.data());
    postman->setScene(scene.data());
    scene->setArbiter(arbiter.data());
    // Replaces initialize as we have no JobManager in this case
    Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(arbiter.data());

    // WHEN
    tst_Node *root = new tst_Node();
    Qt3DCore::QNode *child = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene.data());
    scene->addObservable(root);

    QList<tst_SimpleObserver *> observers;
    for (int i = 0; i < 5; i++) {
        tst_SimpleObserver *s = new tst_SimpleObserver();
        arbiter->registerObserver(s, root->id());
        observers << s;
    }

    QList<tst_SceneObserver *> sceneObservers;
    for (int i = 0; i < 5; i++) {
        tst_SceneObserver *s = new tst_SceneObserver();
        arbiter->registerSceneObserver(s);
        sceneObservers << s;
    }

    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers)
        QVERIFY(o->lastChange().isNull());
    Q_FOREACH (tst_SceneObserver *s, sceneObservers)
        QVERIFY(s->lastChange().isNull());

    // WHEN
    child->setParent(root);
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeCreated);
    }
    Q_FOREACH (tst_SceneObserver *s, sceneObservers) {
        QVERIFY(!s->lastChange().isNull());
        QVERIFY(s->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    // WHEN
    root->sendComponentAddedNotification();
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::ComponentAdded);
    }
    Q_FOREACH (tst_SceneObserver *s, sceneObservers) {
        QVERIFY(!s->lastChange().isNull());
        QVERIFY(s->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    Qt3DCore::QChangeArbiter::destroyThreadLocalChangeQueue(arbiter.data());
}

void tst_QChangeArbiter::unregisterObservers()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QChangeArbiter> arbiter(new Qt3DCore::QChangeArbiter());
    QScopedPointer<Qt3DCore::QScene> scene(new Qt3DCore::QScene());
    QScopedPointer<Qt3DCore::QAbstractPostman> postman(new tst_PostManObserver);
    arbiter->setPostman(postman.data());
    arbiter->setScene(scene.data());
    postman->setScene(scene.data());
    scene->setArbiter(arbiter.data());
    // Replaces initialize as we have no JobManager in this case
    Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(arbiter.data());

    // WHEN
    tst_Node *root = new tst_Node();
    Qt3DCore::QNode *child = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene.data());
    scene->addObservable(root);

    QList<tst_SimpleObserver *> observers;
    for (int i = 0; i < 5; i++) {
        tst_SimpleObserver *s = new tst_SimpleObserver();
        arbiter->registerObserver(s, root->id());
        observers << s;
    }

    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers)
        QVERIFY(o->lastChange().isNull());

    // WHEN
    child->setParent(root);
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    // WHEN
    Q_FOREACH (tst_SimpleObserver *o, observers)
        arbiter->unregisterObserver(o, root->id());

    root->sendAllChangesNotification();
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    Qt3DCore::QChangeArbiter::destroyThreadLocalChangeQueue(arbiter.data());
}

void tst_QChangeArbiter::unregisterSceneObservers()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QChangeArbiter> arbiter(new Qt3DCore::QChangeArbiter());
    QScopedPointer<Qt3DCore::QScene> scene(new Qt3DCore::QScene());
    QScopedPointer<Qt3DCore::QAbstractPostman> postman(new tst_PostManObserver);
    arbiter->setPostman(postman.data());
    arbiter->setScene(scene.data());
    postman->setScene(scene.data());
    scene->setArbiter(arbiter.data());
    // Replaces initialize as we have no JobManager in this case
    Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(arbiter.data());

    // WHEN
    tst_Node *root = new tst_Node();
    Qt3DCore::QNode *child = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene.data());
    scene->addObservable(root);

    QList<tst_SimpleObserver *> observers;
    for (int i = 0; i < 5; i++) {
        tst_SimpleObserver *s = new tst_SimpleObserver();
        arbiter->registerObserver(s, root->id());
        observers << s;
    }

    QList<tst_SceneObserver *> sceneObservers;
    for (int i = 0; i < 5; i++) {
        tst_SceneObserver *s = new tst_SceneObserver();
        arbiter->registerSceneObserver(s);
        sceneObservers << s;
    }

    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers)
        QVERIFY(o->lastChange().isNull());
    Q_FOREACH (tst_SceneObserver *s, sceneObservers)
        QVERIFY(s->lastChange().isNull());

    // WHEN
    child->setParent(root);
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeCreated);
    }
    Q_FOREACH (tst_SceneObserver *s, sceneObservers) {
        QVERIFY(!s->lastChange().isNull());
        QVERIFY(s->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    // WHEN
    root->sendComponentAddedNotification();
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::ComponentAdded);
    }
    Q_FOREACH (tst_SceneObserver *s, sceneObservers) {
        QVERIFY(!s->lastChange().isNull());
        QVERIFY(s->lastChange()->type() == Qt3DCore::NodeCreated);
    }

    // WHEN
    child->setParent(Q_NODE_NULLPTR);
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeAboutToBeDeleted);
    }
    Q_FOREACH (tst_SceneObserver *s, sceneObservers) {
        QVERIFY(!s->lastChange().isNull());
        QVERIFY(s->lastChange()->type() == Qt3DCore::NodeAboutToBeDeleted);
    }

    Q_FOREACH (tst_SceneObserver *s, sceneObservers)
        arbiter->unregisterSceneObserver(s);

    // WHEN
    child->setParent(root);
    arbiter->syncChanges();

    // THEN
    Q_FOREACH (tst_SimpleObserver *o, observers) {
        QVERIFY(!o->lastChange().isNull());
        QVERIFY(o->lastChange()->type() == Qt3DCore::NodeCreated);
    }
    Q_FOREACH (tst_SceneObserver *s, sceneObservers) {
        QVERIFY(!s->lastChange().isNull());
        QVERIFY(s->lastChange()->type() == Qt3DCore::NodeAboutToBeDeleted);
    }

    Qt3DCore::QChangeArbiter::destroyThreadLocalChangeQueue(arbiter.data());
}

void tst_QChangeArbiter::distributeFrontendChanges()
{
    // GIVEN
    QScopedPointer<Qt3DCore::QChangeArbiter> arbiter(new Qt3DCore::QChangeArbiter());
    QScopedPointer<Qt3DCore::QScene> scene(new Qt3DCore::QScene());
    QScopedPointer<Qt3DCore::QAbstractPostman> postman(new tst_PostManObserver);
    arbiter->setPostman(postman.data());
    arbiter->setScene(scene.data());
    postman->setScene(scene.data());
    scene->setArbiter(arbiter.data());
    // Replaces initialize as we have no JobManager in this case
    Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(arbiter.data());

    // WHEN
    tst_Node *root = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene.data());
    scene->addObservable(root);

    tst_SimpleObserver *backendAllChangedObserver = new tst_SimpleObserver();
    tst_SimpleObserver *backendNodeAddedObserver = new tst_SimpleObserver();
    tst_SimpleObserver *backendNodeRemovedObserver = new tst_SimpleObserver();
    tst_SimpleObserver *backendNodeUpdatedObserver = new tst_SimpleObserver();
    tst_SimpleObserver *backendComponentAddedObserver = new tst_SimpleObserver();
    tst_SimpleObserver *backendComponentRemovedObserver = new tst_SimpleObserver();

    arbiter->registerObserver(backendAllChangedObserver, root->id());
    arbiter->registerObserver(backendNodeAddedObserver, root->id(), Qt3DCore::NodeAdded);
    arbiter->registerObserver(backendNodeUpdatedObserver, root->id(), Qt3DCore::NodeUpdated);
    arbiter->registerObserver(backendNodeRemovedObserver, root->id(), Qt3DCore::NodeRemoved);
    arbiter->registerObserver(backendComponentAddedObserver, root->id(), Qt3DCore::ComponentAdded);
    arbiter->registerObserver(backendComponentRemovedObserver, root->id(), Qt3DCore::ComponentRemoved);

    arbiter->syncChanges();

    // THEN
    QVERIFY(backendAllChangedObserver->lastChange().isNull());
    QVERIFY(backendNodeAddedObserver->lastChange().isNull());
    QVERIFY(backendNodeUpdatedObserver->lastChange().isNull());
    QVERIFY(backendNodeRemovedObserver->lastChange().isNull());
    QVERIFY(backendComponentAddedObserver->lastChange().isNull());
    QVERIFY(backendComponentRemovedObserver->lastChange().isNull());

    // WHEN
    root->sendNodeAddedNotification();
    arbiter->syncChanges();

    // THEN
    QCOMPARE(backendAllChangedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeUpdatedObserver->lastChanges().count(), 0);
    QCOMPARE(backendNodeRemovedObserver->lastChanges().count(), 0);
    QCOMPARE(backendComponentAddedObserver->lastChanges().count(), 0);
    QCOMPARE(backendComponentRemovedObserver->lastChanges().count(), 0);

    // WHEN
    root->sendNodeUpdatedNotification();
    arbiter->syncChanges();

    // THEN
    QCOMPARE(backendAllChangedObserver->lastChanges().count(), 2);
    QCOMPARE(backendNodeAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeUpdatedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeRemovedObserver->lastChanges().count(), 0);
    QCOMPARE(backendComponentAddedObserver->lastChanges().count(), 0);
    QCOMPARE(backendComponentRemovedObserver->lastChanges().count(), 0);

    // WHEN
    root->sendNodeRemovedNotification();
    arbiter->syncChanges();

    // THEN
    QCOMPARE(backendAllChangedObserver->lastChanges().count(), 3);
    QCOMPARE(backendNodeAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeUpdatedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeRemovedObserver->lastChanges().count(), 1);
    QCOMPARE(backendComponentAddedObserver->lastChanges().count(), 0);
    QCOMPARE(backendComponentRemovedObserver->lastChanges().count(), 0);

    // WHEN
    root->sendComponentAddedNotification();
    arbiter->syncChanges();

    // THEN
    QCOMPARE(backendAllChangedObserver->lastChanges().count(), 4);
    QCOMPARE(backendNodeAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeUpdatedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeRemovedObserver->lastChanges().count(), 1);
    QCOMPARE(backendComponentAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendComponentRemovedObserver->lastChanges().count(), 0);

    // WHEN
    root->sendComponentRemovedNotification();
    arbiter->syncChanges();

    // THEN
    QCOMPARE(backendAllChangedObserver->lastChanges().count(), 5);
    QCOMPARE(backendNodeAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeUpdatedObserver->lastChanges().count(), 1);
    QCOMPARE(backendNodeRemovedObserver->lastChanges().count(), 1);
    QCOMPARE(backendComponentAddedObserver->lastChanges().count(), 1);
    QCOMPARE(backendComponentRemovedObserver->lastChanges().count(), 1);

    // WHEN
    root->sendAllChangesNotification();
    arbiter->syncChanges();

    // THEN
    QCOMPARE(backendAllChangedObserver->lastChanges().count(), 6);
    QCOMPARE(backendNodeAddedObserver->lastChanges().count(), 2);
    QCOMPARE(backendNodeUpdatedObserver->lastChanges().count(), 2);
    QCOMPARE(backendNodeRemovedObserver->lastChanges().count(), 2);
    QCOMPARE(backendComponentAddedObserver->lastChanges().count(), 2);
    QCOMPARE(backendComponentRemovedObserver->lastChanges().count(), 2);

    Qt3DCore::QChangeArbiter::destroyThreadLocalChangeQueue(arbiter.data());
}

void tst_QChangeArbiter::distributeBackendChanges()
{

    // GIVEN
    QScopedPointer<Qt3DCore::QChangeArbiter> arbiter(new Qt3DCore::QChangeArbiter());
    QScopedPointer<Qt3DCore::QScene> scene(new Qt3DCore::QScene());
    QScopedPointer<tst_PostManObserver> postman(new tst_PostManObserver);
    arbiter->setPostman(postman.data());
    arbiter->setScene(scene.data());
    postman->setScene(scene.data());
    scene->setArbiter(arbiter.data());
    // Replaces initialize as we have no JobManager in this case
    Qt3DCore::QChangeArbiter::createThreadLocalChangeQueue(arbiter.data());

    // In order for backend -> frontend changes to work properly,
    // the backend notification should only be sent
    // from a worker thread which has a dedicated ChangeQueue in the
    // ChangeArbiter different than the frontend ChangeQueue. In this
    // test we will only check that the backend has received the frontend notification


    // WHEN
    tst_Node *root = new tst_Node();
    Qt3DCore::QNodePrivate::get(root)->setScene(scene.data());
    scene->addObservable(root);

    tst_BackendObserverObservable *backenObserverObservable = new tst_BackendObserverObservable();
    arbiter->registerObserver(Qt3DCore::QBackendNodePrivate::get(backenObserverObservable), root->id());
    arbiter->scene()->addObservable(Qt3DCore::QBackendNodePrivate::get(backenObserverObservable), root->id());
    Qt3DCore::QBackendNodePrivate::get(backenObserverObservable)->setArbiter(arbiter.data());

    arbiter->syncChanges();

    // THEN
    QVERIFY(root->lastChange().isNull());
    QVERIFY(backenObserverObservable->lastChange().isNull());
    QCOMPARE(backenObserverObservable->lastChanges().count(), 0);

    // WHEN
    root->sendAllChangesNotification();
    arbiter->syncChanges();

    // THEN
    // backend observer receives event from frontend node "root"
    QCOMPARE(root->lastChanges().count(), 0);
    QCOMPARE(postman->lastChanges().count(), 0);
    QCOMPARE(backenObserverObservable->lastChanges().count(), 1);

    // WHEN
    // simulate a worker thread
    QScopedPointer<ThreadedAnswer> answer(new ThreadedAnswer(arbiter.data(), backenObserverObservable));

    QMutex mutex;
    // sends reply from another thread (simulates job thread)
    answer->start();
    mutex.lock();
    waitingForBackendReplyCondition.wait(&mutex);
    mutex.unlock();

    // To verify that backendObserver sent a reply
    arbiter->syncChanges();

    // THEN
    // the repliers should receive it's reply
    QCOMPARE(backenObserverObservable->lastChanges().count(), 2);
    // verify that postMan has received the change
    QCOMPARE(postman->lastChanges().count(), 1);

    // verify correctness of the reply
    Qt3DCore::QBackendScenePropertyChangePtr c = qSharedPointerDynamicCast<Qt3DCore::QBackendScenePropertyChange>(postman->lastChange());
    QVERIFY(!c.isNull());
    QVERIFY(c->targetNode() == root->id());
    qDebug() << c->propertyName();
    QVERIFY(strcmp(c->propertyName(), "Reply") == 0);
    QVERIFY(c->type() == Qt3DCore::NodeUpdated);

    answer->exit();
    answer->wait();
    Qt3DCore::QChangeArbiter::destroyThreadLocalChangeQueue(arbiter.data());
}

QTEST_GUILESS_MAIN(tst_QChangeArbiter)

#include "tst_qchangearbiter.moc"
