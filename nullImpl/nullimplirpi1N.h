/*
 * This software is not subject to copyright protection and is in the public domain.
 */

#ifndef NULLIMPLIRPI1N_H_
#define NULLIMPLIRPI1N_H_

#include "irpi.h"

/*
 * Declare the implementation class of the IRPI IDENT (1:N) Interface
 */
namespace IRPI {
    class NullImplIRPI1N : public IRPI::IdentInterface {
public:

    NullImplIRPI1N();
    ~NullImplIRPI1N() override;

    ReturnStatus
    initializeEnrollmentSession(const std::string &configDir) override;

    ReturnStatus
    createTemplate(
            const Image &img,
            TemplateRole role,
            std::vector<uint8_t> &templ) override;

    ReturnStatus
    finalizeEnrollment(
            const std::vector<std::pair<size_t,std::vector<uint8_t>>> &vtempl) override;

    ReturnStatus
    initializeIdentificationSession(
            const std::string &configDir) override;

    ReturnStatus
    identifyTemplate(const std::vector<uint8_t> &idTemplate,
            const size_t candidateListLength,
            std::vector<Candidate> &candidateList,
            bool &decision) override;

    static std::shared_ptr<IRPI::IdentInterface>
    getImplementation();

private:
    std::string configDir;
    std::string enrollDir;
    std::vector<size_t> labels;
    int counter;
    // Some other members
};
}

#endif /* NULLIMPLIRPI1N_H_ */
