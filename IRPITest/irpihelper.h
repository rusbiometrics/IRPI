#ifndef IRPIHELPER_H
#define IRPIHELPER_H

#include <cmath>
#include <cstring>
#include <iostream>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QImage>
#include <QDir>

#include "irpi.h"

inline std::ostream&
operator<<(
    std::ostream &s,
    const QImage::Format &_format)
{
    switch (_format) {
        case QImage::Format_ARGB32:
            return (s << "Format_ARGB32");
        case QImage::Format_RGB32:
            return (s << "Format_RGB32");
        case QImage::Format_RGB888:
            return (s << "Format_RGB888");
        case QImage::Format_Grayscale8:
            return (s << "Format_Grayscale8");
        default:
            return (s << "Undefined");
    }
}

inline std::ostream&
operator<<(
    std::ostream &s,
    const QString &_qstring)
{
    return s << _qstring.toLocal8Bit().constData();
}

IRPI::Image readimage(const QString &_filename, QImage::Format _mTARgetformat=QImage::Format_RGB888, bool _verbose=false)
{
    if(_verbose)
        std::cout << _filename << std::endl;

    QImage _qimg;
    if(!_qimg.load(_filename)) {
        if(_verbose)
            std::cout << "Can not load or decode!!! Empty image will be returned" << std::endl;
        return IRPI::Image();
    }

    QImage _tmpqimg;
    if(_qimg.format() == _mTARgetformat) {
        _tmpqimg = _qimg;
    } else {
        _tmpqimg = _qimg.convertToFormat(_mTARgetformat);
        if(_verbose)
            std::cout << _qimg.format() << " converted to: " << _tmpqimg.format() << std::endl;
    }
    if(_verbose) {
        std::cout << " Depth: " << _tmpqimg.depth()  << " bits"
                  << " Width:"  << _tmpqimg.width()
                  << " Height:" << _tmpqimg.height() << std::endl;
    }

    // QImage have one specific property - the scanline data is aligned on a 32-bit boundary
    // So I have found that if the line length is not divisible by 4,
    // extra bytes is added to the end of line to make length divisible by 4
    // Read more here: https://bugreports.qt.io/browse/QTBUG-68379?filter=-2
    // As we do not want to copy this extra bytes to IRPI::Image we should throw them out
    int _validbytesperline = _tmpqimg.width()*_tmpqimg.depth() / 8;
    std::shared_ptr<uint8_t>_ptr(new uint8_t[_tmpqimg.height() * _validbytesperline],std::default_delete<uint8_t[]>());
    for(int i = 0; i < _tmpqimg.height(); ++i) {
        std::memcpy(_ptr.get() + i * _validbytesperline,
                    _tmpqimg.constScanLine(i),
                    static_cast<size_t>(_validbytesperline));
    }
    return IRPI::Image(static_cast<uint16_t>(_tmpqimg.width()),
                       static_cast<uint16_t>(_tmpqimg.height()),
                       static_cast<uint8_t>(_tmpqimg.depth()),_ptr);
}

//---------------------------------------------------
void computeFARandFRR(const std::vector<std::vector<IRPI::Candidate>> &_vcandidates, const std::vector<bool> &_vdecisions, const std::vector<size_t> &_vtruelabel, double &_far, double &_frr)
{
    size_t _tp = 0, _tn = 0, _fn = 0, _fp = 0;
    for(size_t i = 0; i < _vcandidates.size(); ++i) {
        // as irpi.h says - most similar entries appear first
        const IRPI::Candidate &_mate = _vcandidates[i][0];
        if(_vdecisions[i] == true) { // Vendor reports that mate has been found
            if(_mate.label == _vtruelabel[i])
                _tp++;
            else
                _fp++;
        } else { // Vendor reports that mate can not be found
            if(_mate.label == _vtruelabel[i])
                _fn++;
            else
                _tn++;
        }
    }
    _far = static_cast<double>(_fp) / (_fp + _tp + 1.e-6);
    _frr = static_cast<double>(_fn) / (_tn + _fn + 1.e-6);
}
//--------------------------------------------------

struct CMCPoint
{
    CMCPoint() : mTPIR(0), rank(0) {}
    double mTPIR; // aka probability of true positive in top rank
    size_t  rank;
};

std::vector<CMCPoint> computeCMC(const std::vector<std::vector<IRPI::Candidate>> &_vcandidates, const std::vector<size_t> &_vtruelabels)
{
    // We need to count only assigned elements
    size_t length = 0;
    const std::vector<IRPI::Candidate> &_candidates = _vcandidates[0];
    for(size_t &i = length; i < _candidates.size(); ++i) {
        if(_candidates[i].isAssigned == false)
            break;
    }
    // Let's count frequencies for ranks
    std::vector<size_t> _vrankfrequency(length,0);
    for(size_t i = 0; i < _vcandidates.size(); ++i) {
        const std::vector<IRPI::Candidate> &_candidates = _vcandidates[i];
        for(size_t j = 0; j < length; ++j) {
            if(_candidates[j].label == _vtruelabels[i]) {
                _vrankfrequency[j]++;
                break;
            }
        }
    }
    // We are ready to save points
    std::vector<CMCPoint> _vCMC(length,CMCPoint());
    for(size_t i = 0; i < length; ++i) {
        _vCMC[i].rank = i + 1;
        for(size_t j = 0; j < _vCMC[i].rank; ++j) {
            _vCMC[i].mTPIR += _vrankfrequency[j];
        }
        _vCMC[i].mTPIR /= _vcandidates.size();
    }
    return _vCMC;
}

QJsonArray serializeCMC(const std::vector<CMCPoint> &_cmc)
{
    QJsonArray _jsonarr;
    for(size_t i = 0; i < _cmc.size(); ++i) {
        QJsonObject _jsonobj;
        _jsonobj["Rank"] = static_cast<qint64>(_cmc[i].rank);
        _jsonobj["TPIR"] = _cmc[i].mTPIR;
        _jsonarr.push_back(qMove(_jsonobj));
    }
    return _jsonarr;
}

//--------------------------------------------------
void showTimeConsumption(qint64 secondstotal)
{
    qint64 days    = secondstotal / 86400;
    qint64 hours   = (secondstotal - days * 86400) / 3600;
    qint64 minutes = (secondstotal - days * 86400 - hours * 3600) / 60;
    qint64 seconds = secondstotal - days * 86400 - hours * 3600 - minutes * 60;
    std::cout << std::endl << "Test has been complited successfully" << std::endl
              << " It took: " << days << " days "
              << hours << " hours "
              << minutes << " minutes and "
              << seconds << " seconds" << std::endl;
}

#endif // IRPIHELPER_H
