/*
 * This software is not subject to copyright protection
 */
 
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>

#include "nullImpl.h"

using namespace std;
using namespace IRPI;

NullImplIRPI1N::NullImplIRPI1N() {}

NullImplIRPI1N::~NullImplIRPI1N() {}

ReturnStatus
NullImplIRPI1N::initializeEnrollmentSession(const string &configDir)
{
    this->configDir = configDir;
    return ReturnStatus(ReturnCode::Success);
}

ReturnStatus
NullImplIRPI1N::createTemplate(
        const Image &face,
        TemplateRole role,
        vector<uint8_t> &templ)
{
    string blurb{"Long time ago in a galaxy far far away..."};

    templ.resize(blurb.size());
    memcpy(templ.data(), blurb.c_str(), blurb.size());

    return ReturnStatus(ReturnCode::Success);
}

ReturnStatus
NullImplIRPI1N::finalizeEnrollment(
        const string &enrollmentDir,
        const string &edbName,
        const string &edbManifestName)
{
    ifstream edbsrc(edbName, ios::binary);
    ofstream edbdest(enrollmentDir+"/mei.edb", ios::binary);
    ifstream manifestsrc(edbManifestName, ios::binary);
    ofstream manifestdest(enrollmentDir+"/mei.manifest", ios::binary);

    edbdest << edbsrc.rdbuf();
    manifestdest << manifestsrc.rdbuf();

    return ReturnCode::Success;
}

ReturnStatus
NullImplIRPI1N::initializeIdentificationSession(
        const string &configDir,
        const string &enrollmentDir)
{
    this->configDir = configDir;
    this->enrollDir = enrollmentDir;

    /* Load stuff from enrollment database into memory, read in
     * configuration files, etc
     */
    /* Read input file */
    auto manifest = enrollmentDir + "/mei.manifest";
    ifstream inputStream(manifest);
    if (!inputStream.is_open()) {
        cerr << "Failed to open stream for " << manifest << "." << endl;
        return ReturnCode::EnrollDirError;
    }

    string id, size, offset;
    while (inputStream >> id >> size >> offset) {
        templateIds.push_back(id);
    }

    return ReturnCode::Success;
}

ReturnStatus
NullImplIRPI1N::identifyTemplate(
        const vector<uint8_t> &idTemplate,
        const uint32_t candidateListLength,
        vector<Candidate> &candidateList,
        bool &decision)
{
    for (unsigned int i=0; i<candidateListLength; i++) {
        candidateList.push_back(Candidate(true, templateIds[i%(templateIds.size()-1)], candidateListLength-i));
    }
    decision = true;
    return ReturnCode::Success;
}

shared_ptr<IdentInterface>
IdentInterface::getImplementation()
{
    return make_shared<NullImplIRPI1N>();
}

