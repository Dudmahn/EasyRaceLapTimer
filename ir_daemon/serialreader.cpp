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
#include "serialreader.h"
#include "buzzer.h"
#include "qextserialport/qextserialenumerator.h"
#include "configuration.h"
#include "hoststation.h"
#include <QDebug>
#include "logger.h"
#include <wiring_pi.h>

#define BUZZER_ACTIVE_TIME_IN_MS 100

SerialReader::SerialReader(QObject *parent) : QObject(parent)
{
    m_pSerialPort = NULL;
    m_bDebug = false;
    m_last_device_check_time = 0;
    m_bDeviceConfigured = false;
}

void SerialReader::setDebug(bool v){
    this->m_bDebug = v;
}

void SerialReader::setup(){
    m_strDevice = Configuration::instance()->serialReaderDevice();

    LOG_INFO(LOG_FACILTIY_COMMON, "using device %s", qPrintable(m_strDevice));
}

void SerialReader::reset(){
    resetTimer();
    m_sensoredTimes.clear();
    LOG_INFOS(LOG_FACILTIY_COMMON, "SerialReader::resetted");
}

void SerialReader::update(){
    bool device_found = false;
    unsigned int cur_time = millis();

    // check for device every 1000ms
    if((cur_time - m_last_device_check_time) > 1000){
        m_last_device_check_time = cur_time;

        QList<QextPortInfo> portList = QextSerialEnumerator::getPorts();
        for(int i = 0; i < portList.size(); i++){
            if(portList[i].portName == m_strDevice){
                device_found = true;
            }
        }

        if(m_pSerialPort == NULL){

            if(device_found){
                LOG_DBG(LOG_FACILTIY_COMMON, "opening device %s", qPrintable(m_strDevice));
                this->m_pSerialPort = new QextSerialPort(m_strDevice, QextSerialPort::EventDriven);
                m_pSerialPort->setBaudRate(BAUD9600);
                m_pSerialPort->setFlowControl(FLOW_OFF);
                m_pSerialPort->setParity(PAR_NONE);
                m_pSerialPort->setDataBits(DATA_8);
                m_pSerialPort->setStopBits(STOP_1);

                if (m_pSerialPort->open(QIODevice::ReadWrite) == true) {
                    connect(m_pSerialPort, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
                    LOG_INFO(LOG_FACILTIY_COMMON, "listening for data on %s", qPrintable(m_pSerialPort->portName()));
                }
                else {
                   LOG_ERROR(LOG_FACILTIY_COMMON, "device failed to open: %s", qPrintable(m_pSerialPort->errorString()));
                }
            }
        }
        else{
            if(device_found){
                if(! m_bDeviceConfigured){
                    // Configure IR decoder for 7-digit mode, and reset race timer
                    resetTimer();
                }
            }
            else{
                LOG_INFO(LOG_FACILTIY_COMMON, "device disconnected %s", qPrintable(m_strDevice));
                delete(m_pSerialPort);
                m_pSerialPort = NULL;
                m_bDeviceConfigured = false;
            }
        }
    }
}

void SerialReader::onReadyRead()
{
    QByteArray bytes;
    int a = this->m_pSerialPort->bytesAvailable();
    bytes.resize(a);
    this->m_pSerialPort->read(bytes.data(), bytes.size());

    LOG_DBG(LOG_FACILTIY_COMMON, "SerialReader::onReadyRead() bytes read: %d", bytes.size());
    LOG_DBG(LOG_FACILTIY_COMMON, "SerialReader::onReadyRead() bytes: %s", qPrintable(bytes.toHex()));

    m_strIncommingData.append(QString(bytes).replace("\r","").replace("\n",""));

    if(m_strIncommingData.contains("\x01")){
        QStringList list = m_strIncommingData.split("\x01");
        for(int i=0; i < list.length(); i++){
            QString t = list[i];
            if(t.length() > 0){
                this->processCmdString(t);
                m_strIncommingData = m_strIncommingData.remove(0,t.length()+1);
            }
        }
    }
}

void SerialReader::write(QString data){
    if(this->m_pSerialPort->isWritable()){
        this->m_pSerialPort->write(data.toLatin1());
    }
}

void SerialReader::processCmdString(QString data){

    //LOG_DBG(LOG_FACILTIY_COMMON, "SerialReader::processCmdString() bytes: %s", qPrintable(data));

    QStringList fields = data.split("\t");

    if(fields.length() == 4){
        if(fields[0] == "\x25"){
            if(! m_bDeviceConfigured){
                LOG_DBGS(LOG_FACILTIY_COMMON, "serial device configured");
                m_bDeviceConfigured = true;
            }
        }
        else if(fields[0] == "@"){
            if(m_bDeviceConfigured){
                QString token = fields[2];
                QStringList timeStr = fields[3].split(".");
                if(timeStr.length() == 2){
                    unsigned int ms = (unsigned int) (timeStr[0].toInt() * 1000);
                    ms += (unsigned int) (timeStr[1].toInt());

                    HostStation::instance()->setLastScannedToken(token);

                    if(m_sensoredTimes[token] == 0){
                        m_sensoredTimes[token] = ms;
                        LOG_DBG(LOG_FACILTIY_COMMON, "first scan token=%s, ms=%u", qPrintable(token), ms);
                    }else if((m_sensoredTimes[token] + 1000) < ms){
                        unsigned int delta_time = ms - m_sensoredTimes[token];
                        Buzzer::instance()->activate(BUZZER_ACTIVE_TIME_IN_MS);

                        LOG_DBG(LOG_FACILTIY_COMMON, "emitting token=%s, delta_time=%u (ms=%u)", qPrintable(token), delta_time, ms);
                        emit newLapTimeEvent(token, delta_time);
                        m_sensoredTimes[token] = ms;
                    }
                }
            }
        }
    }
}

void SerialReader::resetTimer(){
    // Configure IR decoder for 7-digit mode, and reset race timer
    m_pSerialPort->write("\x01\x25\r\n");
}

