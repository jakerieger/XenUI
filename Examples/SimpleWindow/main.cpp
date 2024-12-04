// Author: Jake Rieger
// Created: 12/3/24.
//

#define XEN_IMPL_APP
#include <App.hpp>

using namespace Xen;

class SimpleWindow final : public IApp {
public:
};

int main() {
    const SimpleWindow app;
    return app.Run();
}