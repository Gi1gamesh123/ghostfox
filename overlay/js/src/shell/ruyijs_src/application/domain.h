#pragma once

// Reuse the imported domain owners without introducing a second model.
namespace ruyijs::domain {}
namespace ruyijs::application {
namespace domain = ::ruyijs::domain;
}
