#ifndef IDUNIFICATION_H
#define IDUNIFICATION_H
#pragma once
#include <QString>
#include <QHash>
#include <QMutex>
#include <QAtomicInt>
#include <QList>
#include <QDebug>

class IDunification
{
public:
	IDunification() : nextGlobalID(101) {}
	int getUnifiedID(int localCamID, int localTrackID)
	{
		QMutexLocker locker(&mutex);
		QString localKey = QString("%1-%2").arg(localCamID).arg(localTrackID);

		if (!localToGlobalMap.contains(localKey))
		{
			int newGlobalID = nextGlobalID.fetchAndAddOrdered(1);
			localToGlobalMap.insert(localKey, newGlobalID);
			globalToLocalMap.insert(newGlobalID, { localKey });
			return newGlobalID;
		}

		return localToGlobalMap.value(localKey);
	}

	void unify(int camA, int trackA, int camB, int trackB)
	{
		QMutexLocker locket(&mutex);
		QString keyA = QString("%1-%2").arg(camA).arg(trackA);
		QString keyB = QString("%1-%2").arg(camB).arg(trackB);

		int globalA = localToGlobalMap.value(keyA, -1);
		int globalB = localToGlobalMap.value(keyB, -1);

		if (globalA != -1 && globalB != -1 && globalA != globalB)
		{
			int idToKeep = qMin(globalA, globalB);
			int idToMerge = qMax(globalA, globalB);

		//	qDebug() << "Unifying global ID";

			QList<QString> keysToRemap = globalToLocalMap.take(idToMerge);

			for (const QString& key : keysToRemap)
			{
				localToGlobalMap[key] = idToKeep;
				globalToLocalMap[idToKeep].append(key);
			}
		}
	}

	void clear()
	{
		QMutexLocker locker(&mutex); 
		localToGlobalMap.clear();
		globalToLocalMap.clear();
		nextGlobalID.store(101);
	}

private:
	QMutex mutex; 
	QAtomicInt nextGlobalID;
	QHash<QString, int> localToGlobalMap;
	QHash<int, QList<QString>> globalToLocalMap;

};
#endif
