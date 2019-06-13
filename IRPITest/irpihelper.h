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
    _far = static_cast<double>(_fp) / (_fp + _tp + 1.e-10);
    _frr = static_cast<double>(_fn) / (_tn + _fn + 1.e-10);
}
//--------------------------------------------------

struct CMCPoint
{
    CMCPoint() : mTPIR(0), rank(0) {}
    double mTPIR; // aka probability of true positive in top rank
    size_t  rank;
};

std::vector<CMCPoint> computeCMC(const std::vector<std::vector<IRPI::Candidate>> &_vcandidates, const std::vector<size_t> &_vtruelabels, const size_t _enrolllabelmax)
{
    if(_vcandidates.size() == 0)
        return std::vector<CMCPoint>();
    // How many ranks can be computed
    size_t ranks = _vcandidates[0].size();
    // Let's count frequencies for ranks
    std::vector<size_t> _vrankfrequency(ranks,0);
    size_t _instances = 0;
    for(size_t i = 0; i < _vcandidates.size(); ++i) {
        if(_vtruelabels[i] <= _enrolllabelmax) { // as we need to count only instances with mates
            _instances++;
            const std::vector<IRPI::Candidate> &_candidates = _vcandidates[i];
            for(size_t j = 0; j < ranks; ++j) {
                if(_candidates[j].isAssigned && (_candidates[j].label == _vtruelabels[i])) {
                    _vrankfrequency[j]++;
                    break;
                }
            }
        }
    }
    // We are ready to compute curve points
    std::vector<CMCPoint> _vCMC(ranks,CMCPoint());
    for(size_t i = 0; i < ranks; ++i) {
        _vCMC[i].rank = i + 1;
        for(size_t j = 0; j < _vCMC[i].rank; ++j) {
            _vCMC[i].mTPIR += _vrankfrequency[j];
        }
        _vCMC[i].mTPIR /= _instances + 1.e-10; // add epsilon here to prevent nan when _instances == 0
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
struct DETPoint
{
    DETPoint() : mFNIR(0), mFPIR(0) {}
    double mFNIR, mFPIR;
};


QJsonArray serializeDET(const std::vector<DETPoint> &_det)
{
    QJsonArray _jsonarr;
    for(size_t i = 0; i < _det.size(); ++i) {
        QJsonObject _jsonobj;
        _jsonobj["FPIR"] = _det[i].mFPIR;
        _jsonobj["FNIR"] = _det[i].mFNIR;
        _jsonarr.push_back(qMove(_jsonobj));
    }
    return _jsonarr;
}

std::vector<DETPoint> computeDET(const std::vector<std::vector<IRPI::Candidate>> &_vcandidates, const std::vector<size_t> &_vtruelabels, const size_t _enrolllabelmax, const size_t _points)
{
    if(_vcandidates.size() == 0 || _points == 0)
        return std::vector<DETPoint>();
    // First we need to find min and max similarity scores
    double _maxsimilarity = std::numeric_limits<double>::min(), _minsimilarity = std::numeric_limits<double>::max();
    for(size_t i = 0; i < _vcandidates.size(); ++i) {
        if(_vcandidates[i].size() > 0) {
            // We know that most similar candidates should go first
            if(_vcandidates[i][0].isAssigned && (_maxsimilarity < _vcandidates[i][0].similarityScore))
                _maxsimilarity = _vcandidates[i][0].similarityScore;
            for(size_t j = 1; j < _vcandidates[i].size(); ++j) {
                if(_vcandidates[i][j].isAssigned && (_minsimilarity > _vcandidates[i][j].similarityScore))
                    _minsimilarity = _vcandidates[i][j].similarityScore;
            }
        }
    }
    _minsimilarity *= 0.99;
    _maxsimilarity *= 1.01;
    const double _similaritystep = (_maxsimilarity - _minsimilarity) / _points;
    std::vector<DETPoint> _curve(_points,DETPoint());
    // Let's compute FPIR and FNIR1 for every similarity step
    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(_points); ++i) { // openmp demands signed integral type to be used
        const double _threshold = _minsimilarity + i*_similaritystep;
        size_t _nonmate_searches = 0, _mate_searches = 0, _similar_nonmate = 0, _unsimilar_mate = 0;
        for(size_t k = 0; k < _vcandidates.size(); ++k) {
            if(_vcandidates[k].size() > 0) {
                if(_vtruelabels[k] <= _enrolllabelmax) { // should have mate in enrollment set, goes to FNIR
                    _mate_searches++;
                    if(_vcandidates[k][0].isAssigned && (_vcandidates[k][0].similarityScore < _threshold))
                        _unsimilar_mate++;
                } else { // no mate, goes to FPIR
                    _nonmate_searches++;
                    if(_vcandidates[k][0].isAssigned && (_vcandidates[k][0].similarityScore >= _threshold))
                        _similar_nonmate++;
                }
            }
        }
        _curve[i].mFNIR = static_cast<double>(_unsimilar_mate) / (_mate_searches + 1.e-10);
        _curve[i].mFPIR = static_cast<double>(_similar_nonmate) / (_nonmate_searches + 1.e-10);
    }
    return _curve;
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
