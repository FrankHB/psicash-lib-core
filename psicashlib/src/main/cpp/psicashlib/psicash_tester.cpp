#include <string>
#include <thread>
#include "psicash_tester.h"
#include "http_status_codes.h"

using namespace std;
using namespace psicash;


namespace testing {

#define TEST_HEADER "X-PsiCash-Test"

// Making this a global rather than PsiCashTester member, because it needs to be modified
// inside a const method. (Tests are not multithreaded, so this is okay. But still ugly.)
static std::vector<std::string> g_request_mutators;


PsiCashTester::PsiCashTester() {
    g_request_mutators.clear();
}

PsiCashTester::~PsiCashTester() {
}

UserData& PsiCashTester::user_data() {
    return *user_data_;
}

error::Error PsiCashTester::MakeRewardRequests(const std::string& transaction_class,
                                               const std::string& distinguisher,
                                               int repeat) {
    for (int i = 0; i < repeat; ++i) {
        if (i != 0) {
            // Sleep a bit to avoid server DB transaction conflicts
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        auto result = MakeHTTPRequestWithRetry(
                "POST", "/transaction", true,
                {{"class",         transaction_class},
                 {"distinguisher", distinguisher}});
        if (!result) {
            return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
        } else if (result->status != kHTTPStatusOK) {
            return MakeError(utils::Stringer("1T reward request failed: ", result->status, "; ",
                                             result->error, "; ", result->body));
        }
    }
    return error::nullerr;
}

error::Result<string>
PsiCashTester::BuildRequestParams(const std::string& method, const std::string& path,
                                  bool include_auth_tokens,
                                  const std::vector<std::pair<std::string, std::string>>& query_params,
                                  int attempt,
                                  const std::map<std::string, std::string>& additional_headers) const {
    auto bonus_headers = additional_headers;
    if (!g_request_mutators.empty()) {
        auto mutator = g_request_mutators.back();
        if (!mutator.empty()) {
            bonus_headers[TEST_HEADER] = mutator;
        }
        g_request_mutators.pop_back();
    }

    return PsiCash::BuildRequestParams(method, path, include_auth_tokens, query_params, attempt,
                                       bonus_headers);
}

bool PsiCashTester::MutatorsEnabled() {
    static bool checked = false;
    if (checked) {
        return mutators_enabled_;
    }
    checked = true;

    SetRequestMutators({"CheckEnabled"});
    auto result = MakeHTTPRequestWithRetry(
            "GET", "/refresh-state", false, {});
    if (!result) {
        throw std::runtime_error("MUTATOR CHECK FAILED: "s + result.error().ToString());
    }

    mutators_enabled_ = (result->status == kHTTPStatusAccepted);

    if (!mutators_enabled_) {
        //cout << "SKIPPING MUTATOR TESTS; status: " << result->status << endl;
    }

    return mutators_enabled_;
}

void PsiCashTester::SetRequestMutators(const std::vector<std::string>& mutators) {
    // We're going to store it reversed so we can pop off the end.
    g_request_mutators.assign(mutators.crbegin(), mutators.crend());
}

} // namespace psicash