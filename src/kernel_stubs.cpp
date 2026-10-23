// Kernel exports the SDK does not implement. Guardian Heroes imports the Xbox Live Vision camera API
// (XUsbcam*); no camera is attached, so Create fails and the rest report an empty state.
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/types.h>

REX_EXPORT_STUB_RETURN(__imp__XUsbcamCreate, 0x80070490)  // ERROR_NOT_FOUND
REX_EXPORT_STUB_RETURN(__imp__XUsbcamDestroy, 0)
REX_EXPORT_STUB_RETURN(__imp__XUsbcamSetConfig, 0x80070490)
REX_EXPORT_STUB_RETURN(__imp__XUsbcamSetView, 0x80070490)
REX_EXPORT_STUB_RETURN(__imp__XUsbcamSetCaptureMode, 0x80070490)
REX_EXPORT_STUB_RETURN(__imp__XUsbcamReadFrame, 0x80070490)
REX_EXPORT_STUB_RETURN(__imp__XUsbcamGetState, 0)
