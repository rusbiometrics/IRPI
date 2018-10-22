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
    size_t itpp = 1, etpp = 1, cmcpoints = 512, candidates = 16;
    bool verbose = false, rewriteoutput = false;
    std::string apiresourcespath;
    QImage::Format qimgtargetformat = QImage::Format_RGB888;
    // If no args passed, show help
    if(argc == 1) {
        std::cout << APP_NAME << " version " << APP_VERSION << std::endl;
        std::cout << "Options:" << std::endl
                  << "\t-g - force to open all images in 8-bit grayscale mode, if not set all images will be opened in 24-bit rgb color mode" << std::endl
                  << "\t-i - input directory with the images, note that this directory should have irpi-compliant structure" << std::endl
                  << "\t-o - output directory where result will be saved" << std::endl
                  << "\t-r - path where Vendor's API should search resources" << std::endl
                  << "\t-d - set how namy identification templates per person should be created (default: " << itpp << ")" << std::endl
                  << "\t-e - set how namy enrollment templates per person should be created (default: " << etpp << ")" << std::endl
                  << "\t-c - number of the candidates to search (default: " << candidates << ")" << std::endl
                  << "\t-p - set how many points for CMC curve should be computed (default: " << cmcpoints << ")" << std::endl
                  << "\t-s - be more verbose (print all measurements)" << std::endl
                  << "\t-w - force output file to be rewritten if already existed" << std::endl;
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
            case 's':
                verbose = true;
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
            case 'd':
                itpp = QString(++argv[0]).toUInt();
                break;
            case 'e':
                etpp = QString(++argv[0]).toUInt();
                break;
            case 'p':
                cmcpoints = QString(++argv[0]).toUInt();
                break;
            case 'c':
                candidates = QString(++argv[0]).toUInt();
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
        return 5;
    }
    QStringList distractorfiles = indir.entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot);
    const size_t distractors = static_cast<size_t>(distractorfiles.size());
    std::cout << "  Distractor files: " << distractors << std::endl;
    if((validsubdirs*itpp + distractors) == 0) {
        std::cerr << std::endl << "There is 0 identification templates! Test could not be performed! Abort..." << std::endl;
        return 6;
    }
    // We need also check if output file already exists
    QFile outputfile(outdir.absolutePath().append("/%1.json").arg(VENDOR_API_NAME));
    if(outputfile.exists() && (rewriteoutput == false)) {
        std::cerr << "Output file already exists in the target location! Abort...";
        return 7;
    } else if(outputfile.open(QFile::WriteOnly) == false) {
        std::cerr << "Can not open output file for write! Abort...";
        return 8;
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
        return 9;
    }

    std::cout << std::endl << "Starting templates generation..." << std::endl;

    IRPI::Image irpiimg;
    std::vector<std::pair<size_t,std::vector<uint8_t>>> vetempl;
    vetempl.reserve(validsubdirs * etpp);
    double etgentime = 0; // enrollment template gen time holder
    size_t eterrors = 0;  // enrollment template gen errors
    size_t label = 0;

    for(int i = 0; i < subdirs.size(); ++i) {
        QDir _subdir(indir.absolutePath().append("/%1").arg(subdirs.at(i)));
        QStringList _files = _subdir.entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

        if(static_cast<size_t>(_files.size()) >= minfilespp) {
            std::cout << std::endl << "  Label: " << label << " - " << subdirs.at(i) << std::endl;

            for(size_t j = 0; j < etpp; ++j) {
                if(verbose)
                    std::cout << "   - enrollment template: " << _files.at(j) << std::endl;
                irpiimg = readimage(_subdir.absoluteFilePath(_files.at(j)),qimgtargetformat,verbose);
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

    etgentime /= vetempl.size();
    std::cout << "\nEnrollment templates" << std::endl
              << "  Total:   " << vetempl.size() << std::endl
              << "  Errors:  " << eterrors << std::endl
              << "  Avgtime: " << 1e-6 * etgentime << " ms" << std::endl
              << "  Size:    " << vetempl[0].second.size() << " bytes (before finalizaition)" << std::endl;


    std::cout << std::endl << "Finalizing..." << std::endl;
    elapsedtimer.start();
    status = recognizer->finalizeEnrollment(vetempl);
    qint64 finalizetime = elapsedtimer.elapsed();
    std::cout << " Time: " << finalizetime << " ms" << std::endl;
    if(status.code != IRPI::ReturnCode::Success) {
        std::cout << "Vendor's error description: " << status.info << std::endl
                  << "Can not finalize enrollment! Abort..." << std::endl;
        return 10;
    }
    vetempl.clear();
    vetempl.shrink_to_fit();

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
        return 11;
    }

    std::cout << std::endl << "Starting templates generation..." << std::endl;

    std::vector<std::pair<size_t,std::vector<uint8_t>>> vitempl;
    vitempl.reserve(validsubdirs * itpp + static_cast<size_t>(distractorfiles.size()));
    double itgentime = 0; // identification template gen time holder
    size_t iterrors = 0;  // identification template gen errors
    label = 0;

    for(int i = 0; i < subdirs.size(); ++i) {
        QDir _subdir(indir.absolutePath().append("/%1").arg(subdirs.at(i)));
        QStringList _files = _subdir.entryList(filefilters,QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

        if(static_cast<size_t>(_files.size()) >= minfilespp) {
            std::cout << std::endl << "  Label: " << label << " - " << subdirs.at(i) << std::endl;

            for(size_t j = etpp; j < minfilespp; ++j) {
                if(verbose)
                    std::cout << "   - identification template: " << _files.at(j) << std::endl;
                irpiimg = readimage(_subdir.absoluteFilePath(_files.at(j)),qimgtargetformat,verbose);
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
                    vitempl.push_back(std::make_pair(label,std::move(_templ)));
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
            vitempl.push_back(std::make_pair(label,std::move(_templ)));
        }
        label++;
    }

    itgentime /= vitempl.size();
    std::cout << "\nIdentification templates" << std::endl
              << "  Total:   " << vitempl.size() << std::endl
              << "  Errors:  " << iterrors << std::endl
              << "  Avgtime: " << 1e-6 * itgentime << " ms" << std::endl
              << "  Size:    " << vitempl[0].second.size() << " bytes" << std::endl;


    //----------------------------------------------------------------
    std::cout << std::endl << "Stage 3 - identification search" << std::endl;
    double matchtime = 0;
    std::vector<std::pair<bool,std::vector<IRPI::Candidate>>> vpredictions;
    for(size_t i = 0; i < vitempl.size(); ++i) {
        std::cout << "  searching for template #" << i << std::endl;
        std::vector<IRPI::Candidate> vcandidates;
        bool decision = false;
        elapsedtimer.start();
        status = recognizer->identifyTemplate(vitempl[i].second,candidates,vcandidates,decision);
        matchtime += elapsedtimer.nsecsElapsed();
        if(status.code != IRPI::ReturnCode::Success) {
            if(verbose) {
                std::cout << "   " << status.code << std::endl;
                std::cout << "   " << status.info << std::endl;
            }
        } else {
            vpredictions.push_back(std::make_pair(decision,std::move(vcandidates)));
        }
    }




    return 0;
}
