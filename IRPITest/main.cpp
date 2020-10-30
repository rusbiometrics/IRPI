#include <iostream>

#include "irpihelper.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    setlocale(LC_CTYPE,"Rus");
#endif
    // Default input values
    QDir indir, outdir;
    indir.setPath(""); outdir.setPath("");
    size_t itpp = 1, etpp = 1, candidates = 64, detpoints = 10000;
    bool verbose = false, rewriteoutput = false, enabledistractors = false, shuffletemplates = false;
    std::string apiresourcespath;
    QImage::Format qimgtargetformat = QImage::Format_RGB888;
    // If no args passed, show help
    if(argc == 1) {
        std::cout << APP_NAME << " version " << APP_VERSION << std::endl;
        std::cout << "Options:" << std::endl
                  << "\t-g      - force to open all images in 8-bit grayscale mode, if not set all images will be opened in 24-bit rgb color mode" << std::endl
                  << "\t-i[str] - input directory with the images, note that this directory should have irpi-compliant structure" << std::endl
                  << "\t-o[str] - output directory where result will be saved" << std::endl
                  << "\t-r[str] - path where Vendor's API should search resources" << std::endl
                  << "\t-n[int] - set how namy identification templates per person should be created (default: " << itpp << ")" << std::endl
                  << "\t-e[int] - set how namy enrollment templates per person should be created (default: " << etpp << ")" << std::endl
                  << "\t-d      - enable search of distractors" << std::endl
                  << "\t-c[int] - number of the candidates to search (default: " << candidates << ")" << std::endl
                  << "\t-p[int] - number of points to compute DET curve (default: " << detpoints << ")" << std::endl
                  << "\t-b      - be more verbose (print all measurements)" << std::endl
                  << "\t-s      - shuffle templates before identification" << std::endl
                  << "\t-w      - force output file to be rewritten if already existed" << std::endl;
        return 0;
    }
    // Let's parse user's command input
    while((--argc > 0) && ((*++argv)[0] == '-'))
        switch(*++argv[0]) {
            case 'g':
                qimgtargetformat = QImage::Format_Grayscale8;
                break;
            case 'w':
                rewriteoutput = true;
                break;
            case 'b':
                verbose = true;
                break;
            case 's':
                shuffletemplates = true;
                break;
            case 'i':
                indir.setPath(++argv[0]);
                break;
            case 'o':
                outdir.setPath(++argv[0]);
                break;
            case 'r':
                apiresourcespath = ++argv[0];
                break;
            case 'n':
                itpp = QString(++argv[0]).toUInt();
                break;
            case 'e':
                etpp = QString(++argv[0]).toUInt();
                break;           
            case 'c':
                candidates = QString(++argv[0]).toUInt();
                break;
            case 'p':
                detpoints = QString(++argv[0]).toUInt();
                break;
            case 'd':
                enabledistractors = true;
                break;
        }
    // Let's check if user have provided valid paths?
    if(indir.absolutePath().isEmpty()) {
        std::cerr << "Empty input directory path! Abort...";
        return 1;
    }
    if(outdir.absolutePath().isEmpty()) {
        std::cerr << "Empty output directory path! Abort...";
        return 2;
    }
    if(!indir.exists()) {
        std::cerr << "Input directory you've provided does not exists! Abort...";
        return 3;
    }
    if(!outdir.exists()) {
        outdir.mkpath(outdir.absolutePath());
        if(!outdir.exists()) {
            std::cerr << "Can not create output directory in the path you've provided! Abort...";
            return 4;
        }
    }
    // Let's check candidates number
    if(candidates < 1) {
        std::cerr << "Number of candidates should be greater than zero! Abort...";
        return 5;
    }
    // Let's check det points number
    if(detpoints < 1) {
        std::cerr << "Number of DET points should be greater than zero! Abort...";
        return 6;
    }
    // Ok we can go forward
    std::cout << "Input dir:\t" << indir.absolutePath().toStdString() << std::endl;
    std::cout << "Output dir:\t" << outdir.absolutePath().toStdString() << std::endl;
    // Let's also check if structure of the input directory is valid
    QDateTime startdt(QDateTime::currentDateTime());
    std::cout << std::endl << "Stage 1 - input directory parsing" << std::endl;
    QStringList subdirs = indir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    std::cout << "  Total subdirs: " << subdirs.size() << std::endl;
    size_t validsubdirs = 0;
    QStringList filefilters;
    filefilters << "*.jpg" << "*.jpeg" << "*.gif" << "*.png" << ".bmp";
    const size_t minfilespp = (itpp == 0 ? etpp : etpp + itpp);
    for(int i = 0; i < subdirs.size(); ++i) {
        QStringList _files = QDir(indir.absolutePath().append("/%1").arg(subdirs.at(i))).entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot);
        if(static_cast<uint>(_files.size()) >= minfilespp) {
            validsubdirs++;
        }
    }
    std::cout << "  Valid subdirs: " << validsubdirs << std::endl;
    if(validsubdirs*etpp == 0) {
        std::cerr << std::endl << "There is 0 enrollment templates! Test could not be performed! Abort..." << std::endl;
        return 7;
    }

    QStringList distractorfiles;
    if(enabledistractors) {
        distractorfiles = indir.entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot);
    }
    const size_t distractors = static_cast<size_t>(distractorfiles.size());
    std::cout << "  Distractor files: " << distractors << std::endl;
    if((validsubdirs*itpp + distractors) == 0) {
        std::cerr << std::endl << "There is 0 identification templates! Test could not be performed! Abort..." << std::endl;
        return 8;
    }
    // We need also check if output file already exists
    QFile outputfile(outdir.absolutePath().append("/%1.json").arg(VENDOR_API_NAME));
    if(outputfile.exists() && (rewriteoutput == false)) {
        std::cerr << "Output file already exists in the target location! Abort...";
        return 9;
    } else if(outputfile.open(QFile::WriteOnly) == false) {
        std::cerr << "Can not open output file for write! Abort...";
        return 10;
    }

    //----------------------------------------------------------------
    std::cout << std::endl << "Stage 2 - enrollment templates generation" << std::endl;
    std::shared_ptr<IRPI::IdentInterface> recognizer = IRPI::IdentInterface::getImplementation();
    std::cout << "  Initializing Vendor's API: ";
    QElapsedTimer elapsedtimer;
    elapsedtimer.start();
    IRPI::ReturnStatus status = recognizer->initializeEnrollmentSession(apiresourcespath);
    qint64 einittimems = elapsedtimer.elapsed();
    std::cout << status.code << std::endl;
    std::cout << " Time: " << einittimems << " ms" << std::endl;
    if(status.code != IRPI::ReturnCode::Success) {
        std::cout << "Vendor's error description: " << status.info << std::endl
                  << "Can not initialize Vendor's API! Abort..." << std::endl;
        return 11;
    }

    std::cout << std::endl << "Starting templates generation..." << std::endl;

    IRPI::Image irpiimg;
    std::vector<std::pair<size_t,std::vector<uint8_t>>> vetempl;     
    vetempl.reserve(validsubdirs * etpp);
    double etgentime = 0; // enrollment template gen time holder
    size_t eterrors = 0;  // enrollment template gen errors
    size_t label = 1;     // need to start from 1 because 0 reserved for default value in IRPI::Candidate

    for(int i = 0; i < subdirs.size(); ++i) {
        QDir _subdir(indir.absolutePath().append("/%1").arg(subdirs.at(i)));
        QStringList _files = _subdir.entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

        if(static_cast<size_t>(_files.size()) >= minfilespp) {
            std::cout << std::endl << "  Label: " << label << " - " << subdirs.at(i) << std::endl;

            for(size_t j = 0; j < etpp; ++j) {
                if(verbose)
                    std::cout << "   - enrollment template: " << _files.at(static_cast<int>(j)) << std::endl;
                irpiimg = readimage(_subdir.absoluteFilePath(_files.at(static_cast<int>(j))),qimgtargetformat,verbose);
                std::vector<uint8_t> _templ;
                elapsedtimer.start();
                status = recognizer->createTemplate(irpiimg,IRPI::TemplateRole::Enrollment_1N,_templ);
                etgentime += elapsedtimer.nsecsElapsed();
                if(status.code != IRPI::ReturnCode::Success) {
                    eterrors++;
                    if(verbose) {
                        std::cout << "   " << status.code << std::endl;
                        std::cout << "   " << status.info << std::endl;
                    }
                } else {
                    vetempl.push_back(std::make_pair(label,std::move(_templ)));
                }
            }
        }
        label++;
    }

    const size_t enrolllabelmax = label - 1; // we will use this when CMC and DET will be computed
    etgentime /= vetempl.size();
    const size_t enrolltemplsizebytes = vetempl[0].second.size();
    std::cout << "\nEnrollment templates" << std::endl
              << "  Total:   " << validsubdirs*etpp << std::endl
              << "  Errors:  " << eterrors << std::endl
              << "  Avgtime: " << 1e-6 * etgentime << " ms" << std::endl
              << "  Size:    " << enrolltemplsizebytes << " bytes (before finalizaition)" << std::endl;


    std::cout << std::endl << "Finalizing..." << std::endl;
    elapsedtimer.start();
    status = recognizer->finalizeEnrollment(vetempl);
    qint64 finalizetimems = elapsedtimer.elapsed();
    std::cout << " Time: " << finalizetimems << " ms" << std::endl;
    if(status.code != IRPI::ReturnCode::Success) {
        std::cout << "Vendor's error description: " << status.info << std::endl
                  << "Can not finalize enrollment! Abort..." << std::endl;
        return 12;
    }
    // As we need not enroll templates any longer, let's release memory occupied by them
    vetempl.clear(); vetempl.shrink_to_fit();

    //----------------------------------------------------------------
    std::cout << std::endl << "Stage 3 - identification templates generation" << std::endl;
    std::cout << "  Initializing Vendor's API: ";
    elapsedtimer.start();
    status = recognizer->initializeIdentificationSession(apiresourcespath);
    qint64 iinittimems = elapsedtimer.elapsed();
    std::cout << status.code << std::endl;
    std::cout << " Time: " << iinittimems << " ms" << std::endl;
    if(status.code != IRPI::ReturnCode::Success) {
        std::cout << "Vendor's error description: " << status.info << std::endl
                  << "Can not initialize Vendor's API! Abort..." << std::endl;
        return 13;
    }

    std::cout << std::endl << "Starting templates generation..." << std::endl;

    std::vector<std::vector<uint8_t>> vitempl;
    std::vector<size_t> vtruelabel;
    vitempl.reserve(validsubdirs * itpp + distractors);
    vtruelabel.reserve(validsubdirs * itpp + distractors);
    double itgentime = 0; // identification template gen time holder
    size_t iterrors = 0;  // identification template gen errors
    label = 1;            // need to start from 1 because 0 reserved for default value in IRPI::Candidate

    for(int i = 0; i < subdirs.size(); ++i) {
        QDir _subdir(indir.absolutePath().append("/%1").arg(subdirs.at(i)));
        QStringList _files = _subdir.entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

        if(static_cast<size_t>(_files.size()) >= minfilespp) {
            if(itpp > 0)
                std::cout << std::endl << "  Label: " << label << " - " << subdirs.at(i) << std::endl;

            for(size_t j = etpp; j < minfilespp; ++j) {
                if(verbose)
                    std::cout << "   - identification template: " << _files.at(static_cast<int>(j)) << std::endl;
                irpiimg = readimage(_subdir.absoluteFilePath(_files.at(static_cast<int>(j))),qimgtargetformat,verbose);
                std::vector<uint8_t> _templ;
                elapsedtimer.start();
                status = recognizer->createTemplate(irpiimg,IRPI::TemplateRole::Search_1N,_templ);
                itgentime += elapsedtimer.nsecsElapsed();
                if(status.code != IRPI::ReturnCode::Success) {
                    iterrors++;
                    if(verbose) {
                        std::cout << "   " << status.code << std::endl;
                        std::cout << "   " << status.info << std::endl;
                    }
                } else {
                    vtruelabel.push_back(label);
                    vitempl.push_back(std::move(_templ));
                }
            }
        }
        label++;
    }
    // Also we need process all distractors
    for(int i = 0; i < distractorfiles.size(); ++i) {
        std::cout << std::endl << "  Label: " << label << " - " << distractorfiles.at(i) << std::endl;
        irpiimg = readimage(indir.absoluteFilePath(distractorfiles.at(i)),qimgtargetformat,verbose);
        std::vector<uint8_t> _templ;
        elapsedtimer.start();
        status = recognizer->createTemplate(irpiimg,IRPI::TemplateRole::Search_1N,_templ);
        itgentime += elapsedtimer.nsecsElapsed();
        if(status.code != IRPI::ReturnCode::Success) {
            iterrors++;
            if(verbose) {
                std::cout << "   " << status.code << std::endl;
                std::cout << "   " << status.info << std::endl;
            }
        } else {            
            vtruelabel.push_back(label);
            vitempl.push_back(std::move(_templ));
        }
        label++;
    }

    itgentime /= vitempl.size();
    //const size_t valididenttempl = vitempl.size();
    const size_t identtemplsizebytes = vitempl[0].size();
    std::cout << "\nIdentification templates" << std::endl
              << "  Total:   " << validsubdirs*itpp + distractors
              << "  (distractors: " << distractors << ")" << std::endl
              << "  Errors:  " << iterrors << std::endl
              << "  Avgtime: " << 1.e-6 * itgentime << " ms" << std::endl
              << "  Size:    " << identtemplsizebytes << " bytes" << std::endl;

    // Optional shuffle identification templates
    if(shuffletemplates) {
        std::srand ( unsigned ( std::time(0) ) );
        std::cout << std::endl << "Shuffling templates" << std::endl;
        random_shuffle(vtruelabel.begin(),vtruelabel.end(),vitempl.begin(),vitempl.end());
    }

    std::cout << std::endl << "Stage 4 - identification search" << std::endl;
    double searchtimens = 0;
    std::vector<std::vector<IRPI::Candidate>> vcandidates;
    vcandidates.reserve(vitempl.size());
    std::vector<bool> vdecisions;
    vdecisions.reserve(vitempl.size());
    bool decision;
    for(size_t i = 0; i < vitempl.size(); ++i) {
        std::cout << "  Identification for label: " << vtruelabel[i] << std::endl;
        std::vector<IRPI::Candidate> vprediction;
        decision = false;
        elapsedtimer.start();
        status = recognizer->identifyTemplate(vitempl[i],candidates,vprediction,decision);
        searchtimens += elapsedtimer.nsecsElapsed();
        if(status.code != IRPI::ReturnCode::Success) {
            if(verbose) {
                std::cout << "   " << status.code << std::endl;
                std::cout << "   " << status.info << std::endl;
            }
        } else {
            vdecisions.push_back(decision);
            vcandidates.push_back(std::move(vprediction));
        }
    }

    searchtimens /= vitempl.size();
    std::cout << std::endl << "  Total identifications: " << vitempl.size() << std::endl;
    std::cout << "  Avg identification time: " << searchtimens*1e-3 << " us" << std::endl;
    // As we need not ident templates any longer, let's release memory occupied by them
    vitempl.clear(); vitempl.shrink_to_fit();       

    std::cout << std::endl << "Stage 5 - CMC and DET computation" << std::endl << std::endl;
    const uint confexaples = 3; // how many exaples are needed to count result confident

    std::vector<CMCPoint> vCMC = computeCMC(vcandidates,vtruelabel,enrolllabelmax);
    if(vCMC.size() > 0)
        std::cout << "  Best TPIR[1]: "
                  << QString::number(vCMC[0].mTPIR,'f',-std::ceil(std::log10(static_cast<double>(confexaples)/(validsubdirs * itpp * etpp)))).toStdString()
                  << std::endl;

    double bestFPIR = 1.0, bestFNIR = 1.0;
    std::vector<DETPoint> vDET;
    if(distractors > 0) {
        vDET = computeDET(vcandidates,vtruelabel,enrolllabelmax,detpoints,confexaples);
        bestFPIR = std::exp(std::log(10.0)*std::ceil(std::log10(static_cast<double>(confexaples)/(distractors * etpp))));
        bestFNIR = findFNIR(vDET,bestFPIR);
        std::cout << "  Best FNIR (FPIR): "
                  << QString::number(bestFNIR,'f',-std::ceil(std::log10(static_cast<double>(confexaples)/(validsubdirs * itpp * etpp)))).toStdString()
                  << " ("
                  << QString::number(bestFPIR,'f',-std::ceil(std::log10(static_cast<double>(confexaples)/(distractors * etpp)))).toStdString()
                  << ")" << std::endl;
    }


    QDateTime enddt = QDateTime::currentDateTime();
    // Let's print time consumption
    showTimeConsumption(startdt.secsTo(enddt));
    // In the end we need to serialize test data
    std::cout << " Wait untill output data will be saved..." << std::endl;

    QJsonObject jsonobj;
    jsonobj["Name"]       = VENDOR_API_NAME;
    jsonobj["StartDT"]    = startdt.toString("dd.MM.yyyy hh:mm:ss");
    jsonobj["EndDT"]      = enddt.toString("dd.MM.yyyy hh:mm:ss");
    jsonobj["CMC"]        = serializeCMC(vCMC);
    if(distractors > 0)
        jsonobj["DET"]    = serializeDET(vDET);

    QJsonObject _ejson;
    _ejson["Templates"]   = static_cast<int>(validsubdirs*etpp);
    _ejson["Perperson"]   = static_cast<int>(etpp);
    _ejson["Errors"]      = static_cast<int>(eterrors);
    _ejson["Gentime_ms"]  = 1.e-6 * etgentime;
    _ejson["Size_bytes"]  = static_cast<int>(enrolltemplsizebytes);
    _ejson["Rejection_rate"] = std::max(eterrors / static_cast<double>(validsubdirs*etpp),
                                        confexaples / static_cast<double>(validsubdirs*etpp));
    jsonobj["Enrollment"] = _ejson;
    QJsonObject _ijson;
    _ijson["Templates"]   = static_cast<int>(validsubdirs*itpp);
    _ijson["Perperson"]   = static_cast<int>(itpp);
    _ijson["Distractors"] = static_cast<int>(distractors);
    _ijson["Errors"]      = static_cast<int>(iterrors);
    _ijson["Gentime_ms"]  = 1.e-6 * itgentime;
    _ijson["Size_bytes"]  = static_cast<int>(identtemplsizebytes);
    _ijson["Rejection_rate"] = std::max(iterrors / static_cast<double>(validsubdirs*itpp),
                                        confexaples / static_cast<double>(validsubdirs*itpp));
    jsonobj["Identification"] = _ijson;

    jsonobj["Searchtime_us"] = searchtimens * 1.e-3;
    jsonobj["Einittime_ms"]  = einittimems;
    jsonobj["Efinalizetime_ms"] = finalizetimems;
    jsonobj["Iinittime_ms"]  = iinittimems;
    jsonobj["FNIR"] = bestFNIR;
    jsonobj["FPIR"] = bestFPIR;
    outputfile.write(QJsonDocument(jsonobj).toJson());
    outputfile.close();
    std::cout << " Done" << std::endl;
    return 0;
}
