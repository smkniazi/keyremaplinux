#include "output_device.h"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "keyremaplinux/util/logging.h"

namespace keyremaplinux {

// Dependent on the OS.
static const std::string uinputDevicePaths[] = {"/dev/input/uinput", "/dev/uinput"};

OutputDevice::~OutputDevice() {
  if (outputDescriptor_ > 0) {
    ioctl(outputDescriptor_, UI_DEV_DESTROY);
    close(outputDescriptor_);
  }
}

OutputDevice::OutputDevice() {
  // uInput device is used to create actual device we will be writing to.
  // See http://thiemonge.org/getting-started-with-uinput
  auto uInputPath = FindUInputDevice();
  CHECK(uInputPath != "");
  outputDescriptor_ = open(uInputPath.c_str(), O_WRONLY | O_NONBLOCK);
  CHECK(outputDescriptor_ > 0);

  EnableUInputEvents();
  CreateDeviceFromUInput();
  CHECK(outputDescriptor_ > 0);

  // Initialize syncEvent_
  struct input_event ev;
  syncEvent_ = (input_event*) malloc(sizeof(ev));
  memset(syncEvent_, 0, sizeof(ev));
  syncEvent_->type = EV_SYN;
  syncEvent_->code = 0;
  syncEvent_->value = 0;
}

std::string OutputDevice::FindUInputDevice() {
  for (auto path : uinputDevicePaths) {
    if (access(path.c_str(), F_OK) != -1) {
      return path;
    }
  }
  return "";
}
  
void OutputDevice::EnableUInputEvents() {
  // Enable keys being sent.
  CHECK(!ioctl(outputDescriptor_, UI_SET_EVBIT, EV_KEY));

  // Enable synchronization events being sent.
  CHECK(!ioctl(outputDescriptor_, UI_SET_EVBIT, EV_SYN));

  // Enable all keybord characters.
  for (int key = 0; key < KEY_MAX; key++) {
    int res = ioctl(outputDescriptor_, UI_SET_KEYBIT, key);
    if (res != 0) {
      LOG(WARNING) << "ioctl failed for character " << key << " with code " << res;
    }
  }
}
  
void OutputDevice::CreateDeviceFromUInput() {
  struct uinput_user_dev uidev;
  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-device");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor  = 0x1;
  uidev.id.product = 0x1;
  uidev.id.version = 1;
  CHECK(write(outputDescriptor_, &uidev, sizeof(uidev)) > 0);
  CHECK(!ioctl(outputDescriptor_, UI_DEV_CREATE));
}

void OutputDevice::WriteInputEvent(input_event* event) {
  CHECK(write(outputDescriptor_, event, sizeof(*event)) > 0);
}

void OutputDevice::WriteSyncEvent() {
  CHECK(write(outputDescriptor_, syncEvent_, sizeof(*syncEvent_)) >= 0);
  CHECK(fsync(outputDescriptor_));
}
  
}  // end namespace keyremaplinux
