#include "Authenticator.h"
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <vector>

namespace {
struct PamData {
    const char* password;
};

int pamConvFunc(int numMsg, const struct pam_message** msg, struct pam_response** resp, void* appdataPtr)
{
    (void)msg;
    if (numMsg <= 0) {
        return PAM_CONV_ERR;
    }

    auto* pamData = static_cast<PamData*>(appdataPtr);

    auto responses = std::make_unique<pam_response[]>(numMsg);
    if (!responses) {
        return PAM_CONV_ERR;
    }

    for (int i = 0; i < numMsg; ++i) {
        responses[i].resp = strdup(pamData->password ? pamData->password : "");
        responses[i].resp_retcode = 0;
    }

    *resp = responses.release();
    return PAM_SUCCESS;
}
}

Authenticator::Authenticator()
{
    struct passwd* pw = getpwuid(getuid());
    if (!pw) {
        throw std::runtime_error("Cannot get username.");
    }
    username = pw->pw_name;
}

bool Authenticator::checkPassword(const std::string& password)
{
    pam_handle_t* pamh = nullptr;
    PamData pamData{ password.c_str() };
    struct pam_conv conv = { pamConvFunc, &pamData };

    int result = pam_start("login", username.c_str(), &conv, &pamh);
    if (result != PAM_SUCCESS) {
        return false;
    }

    auto pamDeleter = [&result](pam_handle_t* h) {
        pam_end(h, result);
    };

    std::unique_ptr<pam_handle_t, decltype(pamDeleter)> pamhCleanup(pamh, pamDeleter);

    result = pam_authenticate(pamh, 0);
    if (result == PAM_SUCCESS) {
        result = pam_acct_mgmt(pamh, 0);
    }

    return (result == PAM_SUCCESS);
}
