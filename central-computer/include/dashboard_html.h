#ifndef CENTRAL_COMPUTER_DASHBOARD_HTML_H
#define CENTRAL_COMPUTER_DASHBOARD_HTML_H

namespace submarine {

// The dashboard's entire page (HTML+CSS+JS), embedded as a single string
// so the app is one self-contained binary with no separate asset files to
// ship or find at runtime. Defined in dashboard_html.cpp.
extern const char* const kDashboardHtml;

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_DASHBOARD_HTML_H
