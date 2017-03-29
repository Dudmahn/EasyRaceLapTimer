/**
 * EasyRaceLapTimer - Copyright 2015-2016 by airbirds.de, a project of polyvision UG (haftungsbeschränkt)
 *
 * Author: Alexander B. Bierbrauer
 *
 * This file is part of EasyRaceLapTimer.
 *
 * EasyRaceLapTimer is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * EasyRaceLapTimer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Foobar. If not, see http://www.gnu.org/licenses/.
 **/
#ifndef SERIALREADER_H
#define SERIALREADER_H

#include <QObject>
#include <singleton.h>
#include <QHash>
#include "qextserialport/qextserialport.h"

class SerialReader : public QObject, public Singleton<SerialReader> {
    Q_OBJECT
    Q_DISABLE_COPY(SerialReader)
public:
    explicit SerialReader(QObject *parent = 0);

    void setup();
    void setDebug(bool);
    void update();
    void write(QString);
    void reset();

Q_SIGNALS:
    void startNewRaceEvent();
    void resetEvent();
    void newLapTimeEvent(QString,unsigned int);

public Q_SLOTS:
    void onReadyRead();

private:
    QString m_strDevice;
    QString m_strIncommingData;
    QextSerialPort *m_pSerialPort;
    bool m_bDebug;
    unsigned int m_last_device_check_time;
    bool m_bDeviceConfigured;
    QHash<QString, unsigned int> m_sensoredTimes;

private:
    void processCmdString(QString);
    void resetTimer();

    friend class Singleton<SerialReader>;
};

#endif // SERIALREADER_H
