/**
 ***************************************************************************
 * @file Service.cpp
 *
 * @brief
 *
 * Copyright (C) 2013
 *
 * @author	Francois Ferrand
 * @date	4/2013
 ***************************************************************************
 */

#include <QtGui/QInputDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QProgressDialog>
#include <QtCore/QDebug>

#include "Service.h"
#include "Protocol.h"
#include "EntryConfig.h"
#include "AccessControlDialog.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Uuid.h"
#include "core/PasswordGenerator.h"


Service::Service(DatabaseTabWidget *parent) :
    KeepassHttpProtocol::Server(parent),
    m_dbTabWidget(parent)
{
}

static const unsigned char KEEPASSHTTP_UUID_DATA[] = {
    0x34, 0x69, 0x7a, 0x40, 0x8a, 0x5b, 0x41, 0xc0,
    0x9f, 0x36, 0x89, 0x7d, 0x62, 0x3e, 0xcb, 0x31
};
static const Uuid KEEPASSHTTP_UUID = Uuid(QByteArray::fromRawData(reinterpret_cast<const char *>(KEEPASSHTTP_UUID_DATA), sizeof(KEEPASSHTTP_UUID_DATA)));
static const char KEEPASSHTTP_NAME[] = "KeePassHttp Settings";
static const char ASSOCIATE_KEY_PREFIX[] = "AES Key: ";
static const char KEEPASSHTTP_GROUP_NAME[] = "KeePassHttp Passwords";   //Group where new KeePassHttp password are stored
//private const int DEFAULT_NOTIFICATION_TIME = 5000;

Entry* Service::getConfigEntry(bool create)
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database()) {
            Entry* entry = db->resolveEntry(KEEPASSHTTP_UUID);
            if (!entry && create) {
                entry = new Entry();
                entry->setTitle(QLatin1String(KEEPASSHTTP_NAME));
                entry->setUuid(KEEPASSHTTP_UUID);
                entry->setAutoTypeEnabled(false);
                entry->setGroup(db->rootGroup());
            } else if (entry && entry->group() == db->metadata()->recycleBin()) {
                if (create)
                    entry->setGroup(db->rootGroup());
                else
                    entry = NULL;
            }
            return entry;
        }
    return NULL;
}

bool Service::isDatabaseOpened() const
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        switch(dbWidget->currentMode()) {
        case DatabaseWidget::None:
        case DatabaseWidget::LockedMode:
            break;

        case DatabaseWidget::ViewMode:
        case DatabaseWidget::EditMode:
            return true;
        }
    return false;
}

bool Service::openDatabase()
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (dbWidget->currentMode() == DatabaseWidget::LockedMode) {
            //- show notification
            //- open window
            //- wait a few seconds for user to unlock...
        }
    return false;
}

QString Service::getDatabaseRootUuid()
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database())
            if (Group * rootGroup = db->rootGroup())
                return rootGroup->uuid().toHex();
    return QString();
}

QString Service::getDatabaseRecycleBinUuid()
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database())
            if (Group * recycleBin = db->metadata()->recycleBin())
                return recycleBin->uuid().toHex();
    return QString();
}

QString Service::getKey(const QString &id)
{
    if (Entry* config = getConfigEntry())
        return config->attributes()->value(QLatin1String(ASSOCIATE_KEY_PREFIX) + id);
    return QString();
}

QString Service::storeKey(const QString &key)
{
    QString id;
    if (Entry* config = getConfigEntry(true)) {
        do {
            bool ok;
            //Indicate who wants to associate, and request user to enter the 'name' of association key
            id = QInputDialog::getText(0, tr("KeyPassX/Http: New key association request"),
                                       tr("You have received an association request for the above key. If you would like to "
                                          "allow it access to your KeePassX database give it a unique name to identify and"
                                          "accept it."),
                                       QLineEdit::Normal, QString(), &ok);
            if (!ok || id.isEmpty())
                return QString();

            //Warn if association key already exists
        } while(config->attributes()->contains(QLatin1String(ASSOCIATE_KEY_PREFIX) + id) &&
                QMessageBox::warning(0, tr("KeyPassX/Http: Overwrite existing key?"),
                                     tr("A shared encryption-key with the name \"%1\" already exists.\nDo you want to overwrite it?").arg(id),
                                     QMessageBox::Yes|QMessageBox::No) == QMessageBox::No);

        config->attributes()->set(QLatin1String(ASSOCIATE_KEY_PREFIX) + id, key, true);
    }
    return id;
}

bool Service::matchUrlScheme(const QString & url)
{
    QString str = url.left(8).toLower();
    return str.startsWith("http://") ||
           str.startsWith("https://") ||
           str.startsWith("ftp://") ||
           str.startsWith("ftps://");
}

bool Service::removeFirstDomain(QString & hostname)
{
    int pos = hostname.indexOf(".");
    if (pos < 0)
        return false;
    hostname = hostname.mid(pos + 1);
    return !hostname.isEmpty();
}

QList<Entry*> Service::searchEntries(const QString &text)
{
    QList<Entry*> entries;

    //TODO: setting to search all databases [e.g. as long as the 'current' db is authentified

    //Search entries matching the hostname
    QString hostname = QUrl(text).host();
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database())
            if (Group * rootGroup = db->rootGroup())
                do {
                    Q_FOREACH (Entry * entry, rootGroup->search(hostname, Qt::CaseInsensitive)) {
                        QString title = entry->title();
                        QString url = entry->url();

                        //Filter to match hostname in Title and Url fields
                        if (   hostname.contains(title)
                            || hostname.contains(url)
                            || (matchUrlScheme(title) && hostname.contains(QUrl(title).host()))
                            || (matchUrlScheme(url) && hostname.contains(QUrl(url).host())) )
                            entries.append(entry);
                    }
                } while(entries.isEmpty() && removeFirstDomain(hostname));
    return entries;
}

Service::Access Service::checkAccess(const Entry *entry, const QString & host, const QString & submitHost, const QString & realm)
{
    EntryConfig config;
    if (!config.load(entry))
        return Unknown;  //not configured
    if ((config.isAllowed(host)) && (submitHost.isEmpty() || config.isAllowed(submitHost)))
        return Allowed;  //allowed
    if ((config.isDenied(host)) || (!submitHost.isEmpty() && config.isDenied(submitHost)))
        return Denied;   //denied
    if (!realm.isEmpty() && config.realm() != realm)
        return Denied;
    return Unknown;      //not configured for this host
}

KeepassHttpProtocol::Entry Service::prepareEntry(const Entry* entry)
{
    bool returnStringFields = true; //TODO: setting!
    KeepassHttpProtocol::Entry res(entry->title(), entry->username(), entry->password(), entry->uuid().toHex());
    if (returnStringFields)
    {
        const EntryAttributes * attr = entry->attributes();
        Q_FOREACH (const QString& key, attr->keys())
            if (key.startsWith(QLatin1String("KPH: ")))
                res.addStringField(key, attr->value(key));
    }
    return res;
}

QList<KeepassHttpProtocol::Entry> Service::findMatchingEntries(const QString &id, const QString &url, const QString &submitUrl, const QString &realm)
{
    QList<KeepassHttpProtocol::Entry> result;
    QList<Entry*> pwEntriesToConfirm;

    bool autoAccept = false;        //TODO: setting!
    const QString host = QUrl(url).host();
    const QString submitHost = QUrl(submitUrl).host();
    const QList<Entry*> pwEntries = searchEntries(url);

    //Check entries for authorization
    Q_FOREACH (Entry * entry, pwEntries) {
        switch(checkAccess(entry, host, submitHost, realm)) {
        case Denied:
            continue;

        case Unknown:
            if (!autoAccept) {
                pwEntriesToConfirm.append(entry);
                break;
            }
            //fall through
        case Allowed:
            result << prepareEntry(entry);
            break;
        }
    }

    //If unsure, ask user for confirmation
    if (!pwEntriesToConfirm.isEmpty()) {
        //TODO: balloon to grant access + timeout

        AccessControlDialog dlg;
        dlg.setUrl(url);
        dlg.setItems(pwEntriesToConfirm);
        //dlg.setRemember();        //TODO: setting!

        int res = dlg.exec();
        if (dlg.remember()) {
            Q_FOREACH (Entry * entry, pwEntries) {
                EntryConfig config;
                config.load(entry);
                if (res == QDialog::Accepted) {
                    config.allow(host);
                    if (!submitHost.isEmpty() && host != submitHost)
                        config.allow(submitHost);
                } else if (res == QDialog::Rejected) {
                    config.deny(host);
                    if (!submitHost.isEmpty() && host != submitHost)
                        config.deny(submitHost);
                }
                if (!realm.isEmpty())
                    config.setRealm(realm);
                config.save(entry);
            }
        }
        if (res == QDialog::Accepted) {
            Q_FOREACH (Entry * entry, pwEntries)
                result << prepareEntry(entry);
        }
    }

    //TODO: sort [--> need a flag], or do this in Server class [--> need an extra 'sort order' key in Entry, and we always compute it]
    return result;
}

int Service::countMatchingEntries(const QString &id, const QString &url, const QString &submitUrl, const QString &realm)
{
    return searchEntries(url).count();
}

QList<KeepassHttpProtocol::Entry> Service::searchAllEntries(const QString &id)
{
    QList<KeepassHttpProtocol::Entry> result;
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database())
            if (Group * rootGroup = db->rootGroup())
                Q_FOREACH (Entry * entry, rootGroup->entriesRecursive())
                    result << KeepassHttpProtocol::Entry(entry->title(), entry->username(), QString(), entry->uuid().toHex());
    return result;
}

Group * Service::findCreateAddEntryGroup()
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database())
            if (Group * rootGroup = db->rootGroup()) {
                const QString groupName = QLatin1String(KEEPASSHTTP_GROUP_NAME);//TODO: setting to decide where new keys are created

                Q_FOREACH (const Group * g, rootGroup->groupsRecursive(true))
                    if (g->name() == groupName)
                        return db->resolveGroup(g->uuid());

                Group * group;
                group = new Group();
                group->setUuid(Uuid::random());
                group->setName(groupName);
                group->setIcon(Group::DefaultIconNumber);   //TODO: WorldIconNumber
                group->setParent(rootGroup);
                return group;
            }
    return NULL;
}

void Service::addEntry(const QString &id, const QString &login, const QString &password, const QString &url, const QString &submitUrl, const QString &realm)
{
    if (Group * group = findCreateAddEntryGroup()) {
        Entry * entry = new Entry();
        entry->setUuid(Uuid::random());
        entry->setTitle(QUrl(url).host());
        entry->setUrl(url);
        entry->setIcon(Entry::DefaultIconNumber);           //TODO: WorldIconNumber
        entry->setUsername(login);
        entry->setPassword(password);
        entry->setGroup(group);

        const QString host = QUrl(url).host();
        const QString submitHost = QUrl(submitUrl).host();
        EntryConfig config;
        config.allow(host);
        if (!submitHost.isEmpty())
            config.allow(submitHost);
        if (!realm.isEmpty())
            config.setRealm(realm);
        config.save(entry);
    }
}

void Service::updateEntry(const QString &id, const QString &uuid, const QString &login, const QString &password, const QString &url)
{
    if (DatabaseWidget * dbWidget = m_dbTabWidget->currentDatabaseWidget())
        if (Database * db = dbWidget->database())
            if (Entry * entry = db->resolveEntry(Uuid::fromHex(uuid))) {
                QString u = entry->username();
                if (u != login || entry->password() != password) {
                    bool autoAllow = false;                 //TODO: setting to request confirmation/auto-allow
                    if (   autoAllow
                        || QMessageBox::warning(0, tr("KeyPassX/Http: Update Entry"),
                                                tr("Do you want to update the information in %1 - %2?").arg(QUrl(url).host()).arg(u),
                                                QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes ) {
                        entry->beginUpdate();
                        entry->setUsername(login);
                        entry->setPassword(password);
                        entry->endUpdate();
                    }
                }
            }
}

QString Service::generatePassword()
{
    PasswordGenerator * pwGenerator = passwordGenerator();
    //TODO: password generator settings
    return pwGenerator->generatePassword(20,
                                         PasswordGenerator::LowerLetters | PasswordGenerator::UpperLetters | PasswordGenerator::Numbers,
                                         PasswordGenerator::ExcludeLookAlike | PasswordGenerator::CharFromEveryGroup);
}

void Service::removeSharedEncryptionKeys()
{
    if (!isDatabaseOpened()) {
        QMessageBox::critical(0, tr("KeyPassX/Http: Database locked!"),
                              tr("The active database is locked!\n"
                                 "Please unlock the selected database or choose another one which is unlocked."),
                              QMessageBox::Ok);
    } else if (Entry* entry = getConfigEntry()) {
        QStringList keysToRemove;
        Q_FOREACH (const QString& key, entry->attributes()->keys())
            if (key.startsWith(ASSOCIATE_KEY_PREFIX))
                keysToRemove << key;

        if(keysToRemove.count()) {
            entry->beginUpdate();
            Q_FOREACH (const QString& key, keysToRemove)
                entry->attributes()->remove(key);
            entry->endUpdate();

            const int count = keysToRemove.count();
            QMessageBox::information(0, tr("KeyPassX/Http: Removed keys from database"),
                                     tr("Successfully removed %1 encryption-%2 from KeePassX/Http Settings.").arg(count).arg(count ? "keys" : "key"),
                                     QMessageBox::Ok);
        } else {
            QMessageBox::information(0, tr("KeyPassX/Http: No keys found"),
                                     tr("No shared encryption-keys found in KeePassHttp Settings."),
                                     QMessageBox::Ok);
        }
    } else {
        QMessageBox::information(0, tr("KeyPassX/Http: Settings not available!"),
                                 tr("The active database does not contain an entry of KeePassHttp Settings."),
                                 QMessageBox::Ok);
    }
}

void Service::removeStoredPermissions()
{
    if (!isDatabaseOpened()) {
        QMessageBox::critical(0, tr("KeyPassX/Http: Database locked!"),
                              tr("The active database is locked!\n"
                                 "Please unlock the selected database or choose another one which is unlocked."),
                              QMessageBox::Ok);
    } else {
        Database * db = m_dbTabWidget->currentDatabaseWidget()->database();
        QList<Entry*> entries = db->rootGroup()->entriesRecursive();

        QProgressDialog progress(tr("Removing stored permissions..."), tr("Abort"), 0, entries.count());
        progress.setWindowModality(Qt::WindowModal);

        uint counter = 0;
        Q_FOREACH (Entry* entry, entries) {
            if (progress.wasCanceled())
                return;
            if (entry->attributes()->contains(KEEPASSHTTP_NAME)) {
                entry->beginUpdate();
                entry->attributes()->remove(KEEPASSHTTP_NAME);
                entry->endUpdate();
                counter ++;
            }
            progress.setValue(progress.value() + 1);
        }
        progress.reset();

        if (counter > 0) {
            QMessageBox::information(0, tr("KeyPassX/Http: Removed permissions"),
                                     tr("Successfully removed permissions from %1 %2.").arg(counter).arg(counter ? "entries" : "entry"),
                                     QMessageBox::Ok);
        } else {
            QMessageBox::information(0, tr("KeyPassX/Http: No entry with permissions found!"),
                                     tr("The active database does not contain an entry with permissions."),
                                     QMessageBox::Ok);
        }
    }
}
