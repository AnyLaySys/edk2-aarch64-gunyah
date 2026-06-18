/** @file
  This driver is a implementation of the Graphics Output Protocol
  for the QEMU ramfb device.

  Copyright (c) 2018, Red Hat Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Protocol/GraphicsOutput.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/QemuFwCfgLib.h>

#include <Guid/QemuRamfb.h>

#define RAMFB_MMIO_BASE  0x0F000000ULL
#define RAMFB_MMIO_SIZE  0x01000000ULL
#define RAMFB_FORMAT  0x34325258 /* DRM_FORMAT_XRGB8888 */
#define RAMFB_BPP     4

#pragma pack (1)
typedef struct RAMFB_CONFIG {
  UINT64    Address;
  UINT32    FourCC;
  UINT32    Flags;
  UINT32    Width;
  UINT32    Height;
  UINT32    Stride;
} RAMFB_CONFIG;
#pragma pack ()

STATIC EFI_HANDLE              mRamfbHandle;
STATIC EFI_HANDLE              mGopHandle;
STATIC FIRMWARE_CONFIG_ITEM    mRamfbFwCfgItem;

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  mQemuRamfbModeInfo[] = {
  {
    0,    // Version
    640,  // HorizontalResolution
    480,  // VerticalResolution
  },{
    0,    // Version
    800,  // HorizontalResolution
    600,  // VerticalResolution
  },{
    0,    // Version
    1024, // HorizontalResolution
    768,  // VerticalResolution
  }
};

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  mQemuRamfbMode = {
  ARRAY_SIZE (mQemuRamfbModeInfo),                // MaxMode
  0,                                              // Mode
  mQemuRamfbModeInfo,                             // Info
  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),  // SizeOfInfo
};

STATIC
EFI_STATUS
EFIAPI
QemuRamfbGraphicsOutputQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *ModeInfo;

  if ((Info == NULL) || (SizeOfInfo == NULL) ||
      (ModeNumber >= mQemuRamfbMode.MaxMode))
  {
    return EFI_INVALID_PARAMETER;
  }

  ModeInfo = &mQemuRamfbModeInfo[ModeNumber];

  *Info = AllocateCopyPool (
            sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
            ModeInfo
            );
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
QemuRamfbGraphicsOutputSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN  UINT32                        ModeNumber
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *ModeInfo;
  RAMFB_CONFIG                          Config;

  if (ModeNumber >= mQemuRamfbMode.MaxMode) {
    return EFI_UNSUPPORTED;
  }

  ModeInfo = &mQemuRamfbModeInfo[ModeNumber];

  DEBUG ((
    DEBUG_INFO,
    "Ramfb: SetMode %u (%ux%u)\n",
    ModeNumber,
    ModeInfo->HorizontalResolution,
    ModeInfo->VerticalResolution
    ));

  Config.Address = SwapBytes64 (mQemuRamfbMode.FrameBufferBase);
  Config.FourCC  = SwapBytes32 (RAMFB_FORMAT);
  Config.Flags   = SwapBytes32 (0);
  Config.Width   = SwapBytes32 (ModeInfo->HorizontalResolution);
  Config.Height  = SwapBytes32 (ModeInfo->VerticalResolution);
  Config.Stride  = SwapBytes32 (ModeInfo->HorizontalResolution * RAMFB_BPP);

  mQemuRamfbMode.Mode = ModeNumber;
  mQemuRamfbMode.Info = ModeInfo;

  QemuFwCfgSelectItem (mRamfbFwCfgItem);
  QemuFwCfgWriteBytes (sizeof (Config), &Config);

  return EFI_SUCCESS;
}

STATIC
UINT32
RamfbPixelFromBlt (
  IN CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Pixel
  )
{
  return (UINT32)Pixel->Blue |
         ((UINT32)Pixel->Green << 8) |
         ((UINT32)Pixel->Red << 16);
}

STATIC
VOID
RamfbPixelToBlt (
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Pixel,
  IN  UINT32                         Value
  )
{
  Pixel->Blue     = (UINT8)Value;
  Pixel->Green    = (UINT8)(Value >> 8);
  Pixel->Red      = (UINT8)(Value >> 16);
  Pixel->Reserved = 0;
}

STATIC
UINTN
RamfbPixelAddress (
  IN UINTN  X,
  IN UINTN  Y
  )
{
  return (UINTN)mQemuRamfbMode.FrameBufferBase +
         ((Y * mQemuRamfbMode.Info->PixelsPerScanLine + X) * RAMFB_BPP);
}

STATIC
BOOLEAN
RamfbRectFits (
  IN UINTN  X,
  IN UINTN  Y,
  IN UINTN  Width,
  IN UINTN  Height
  )
{
  if ((Width == 0) || (Height == 0)) {
    return FALSE;
  }

  if ((X > mQemuRamfbMode.Info->HorizontalResolution) ||
      (Y > mQemuRamfbMode.Info->VerticalResolution))
  {
    return FALSE;
  }

  if ((Width > mQemuRamfbMode.Info->HorizontalResolution - X) ||
      (Height > mQemuRamfbMode.Info->VerticalResolution - Y))
  {
    return FALSE;
  }

  return TRUE;
}

STATIC
EFI_STATUS
EFIAPI
QemuRamfbGraphicsOutputBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer  OPTIONAL,
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN  UINTN                              SourceX,
  IN  UINTN                              SourceY,
  IN  UINTN                              DestinationX,
  IN  UINTN                              DestinationY,
  IN  UINTN                              Width,
  IN  UINTN                              Height,
  IN  UINTN                              Delta
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Pixel;
  UINT8                          *Buffer;
  UINTN                          X;
  UINTN                          Y;
  UINTN                          CopyX;
  UINTN                          CopyY;
  INTN                           StepX;
  INTN                           StepY;
  UINT32                         Value;

  if ((This == NULL) || (mQemuRamfbMode.Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  switch (BltOperation) {
    case EfiBltVideoFill:
      if ((BltBuffer == NULL) ||
          !RamfbRectFits (DestinationX, DestinationY, Width, Height))
      {
        return EFI_INVALID_PARAMETER;
      }

      Value = RamfbPixelFromBlt (BltBuffer);
      for (Y = 0; Y < Height; Y++) {
        for (X = 0; X < Width; X++) {
          MmioWrite32 (
            RamfbPixelAddress (DestinationX + X, DestinationY + Y),
            Value
            );
        }
      }

      return EFI_SUCCESS;

    case EfiBltBufferToVideo:
      if ((BltBuffer == NULL) ||
          !RamfbRectFits (DestinationX, DestinationY, Width, Height))
      {
        return EFI_INVALID_PARAMETER;
      }

      if (Delta == 0) {
        Delta = Width * sizeof (*BltBuffer);
      }

      Buffer = (UINT8 *)BltBuffer;
      for (Y = 0; Y < Height; Y++) {
        Pixel = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)
                  (Buffer + (SourceY + Y) * Delta) + SourceX;
        for (X = 0; X < Width; X++) {
          MmioWrite32 (
            RamfbPixelAddress (DestinationX + X, DestinationY + Y),
            RamfbPixelFromBlt (&Pixel[X])
            );
        }
      }

      return EFI_SUCCESS;

    case EfiBltVideoToBltBuffer:
      if ((BltBuffer == NULL) ||
          !RamfbRectFits (SourceX, SourceY, Width, Height))
      {
        return EFI_INVALID_PARAMETER;
      }

      if (Delta == 0) {
        Delta = Width * sizeof (*BltBuffer);
      }

      Buffer = (UINT8 *)BltBuffer;
      for (Y = 0; Y < Height; Y++) {
        Pixel = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)
                  (Buffer + (DestinationY + Y) * Delta) + DestinationX;
        for (X = 0; X < Width; X++) {
          Value = MmioRead32 (RamfbPixelAddress (SourceX + X, SourceY + Y));
          RamfbPixelToBlt (&Pixel[X], Value);
        }
      }

      return EFI_SUCCESS;

    case EfiBltVideoToVideo:
      if (!RamfbRectFits (SourceX, SourceY, Width, Height) ||
          !RamfbRectFits (DestinationX, DestinationY, Width, Height))
      {
        return EFI_INVALID_PARAMETER;
      }

      StepX = 1;
      StepY = 1;
      if (DestinationY > SourceY) {
        StepY = -1;
      }

      if ((DestinationY == SourceY) && (DestinationX > SourceX)) {
        StepX = -1;
      }

      for (Y = 0; Y < Height; Y++) {
        CopyY = (StepY > 0) ? Y : (Height - 1 - Y);
        for (X = 0; X < Width; X++) {
          CopyX = (StepX > 0) ? X : (Width - 1 - X);
          Value = MmioRead32 (
                    RamfbPixelAddress (SourceX + CopyX, SourceY + CopyY)
                    );
          MmioWrite32 (
            RamfbPixelAddress (DestinationX + CopyX, DestinationY + CopyY),
            Value
            );
        }
      }

      return EFI_SUCCESS;

    default:
      return EFI_INVALID_PARAMETER;
  }
}

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  mQemuRamfbGraphicsOutput = {
  QemuRamfbGraphicsOutputQueryMode,
  QemuRamfbGraphicsOutputSetMode,
  QemuRamfbGraphicsOutputBlt,
  &mQemuRamfbMode,
};

EFI_STATUS
EFIAPI
InitializeQemuRamfb (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *RamfbDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *GopDevicePath;
  VOID                      *DevicePath;
  VENDOR_DEVICE_PATH        VendorDeviceNode;
  ACPI_ADR_DEVICE_PATH      AcpiDeviceNode;
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      FbBase;
  UINTN                     FbSize, MaxFbSize, Pages;
  UINTN                     FwCfgSize;
  UINTN                     Index;

  if (!QemuFwCfgIsAvailable ()) {
    DEBUG ((DEBUG_INFO, "Ramfb: no FwCfg\n"));
    return EFI_NOT_FOUND;
  }

  Status = QemuFwCfgFindFile ("etc/ramfb", &mRamfbFwCfgItem, &FwCfgSize);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  if (FwCfgSize != sizeof (RAMFB_CONFIG)) {
    DEBUG ((
      DEBUG_ERROR,
      "Ramfb: FwCfg size mismatch (expected %lu, got %lu)\n",
      (UINT64)sizeof (RAMFB_CONFIG),
      (UINT64)FwCfgSize
      ));
    return EFI_PROTOCOL_ERROR;
  }

  MaxFbSize = 0;
  for (Index = 0; Index < ARRAY_SIZE (mQemuRamfbModeInfo); Index++) {
    mQemuRamfbModeInfo[Index].PixelsPerScanLine =
      mQemuRamfbModeInfo[Index].HorizontalResolution;
    mQemuRamfbModeInfo[Index].PixelFormat =
      PixelBlueGreenRedReserved8BitPerColor;
    FbSize = RAMFB_BPP *
             mQemuRamfbModeInfo[Index].HorizontalResolution *
             mQemuRamfbModeInfo[Index].VerticalResolution;
    if (MaxFbSize < FbSize) {
      MaxFbSize = FbSize;
    }

    DEBUG ((
      DEBUG_INFO,
      "Ramfb: Mode %lu: %ux%u, %lu kB\n",
      (UINT64)Index,
      mQemuRamfbModeInfo[Index].HorizontalResolution,
      mQemuRamfbModeInfo[Index].VerticalResolution,
      (UINT64)(FbSize / 1024)
      ));
  }

  Pages     = EFI_SIZE_TO_PAGES (MaxFbSize);
  MaxFbSize = EFI_PAGES_TO_SIZE (Pages);
  FbBase    = RAMFB_MMIO_BASE;

  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeMemoryMappedIo,
                  RAMFB_MMIO_BASE,
                  RAMFB_MMIO_SIZE,
                  EFI_MEMORY_UC | EFI_MEMORY_XP
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Ramfb: AddMemorySpace failed: %r\n", Status));
    return Status;
  }

  Status = gDS->AllocateMemorySpace (
                  EfiGcdAllocateAddress,
                  EfiGcdMemoryTypeMemoryMappedIo,
                  0,
                  MaxFbSize,
                  &FbBase,
                  ImageHandle,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Ramfb: AllocateMemorySpace failed: %r\n", Status));
    gDS->RemoveMemorySpace (RAMFB_MMIO_BASE, RAMFB_MMIO_SIZE);
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (
                  RAMFB_MMIO_BASE,
                  RAMFB_MMIO_SIZE,
                  EFI_MEMORY_UC | EFI_MEMORY_XP
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Ramfb: SetMemorySpaceAttributes failed: %r\n", Status));
    gDS->FreeMemorySpace (FbBase, MaxFbSize);
    gDS->RemoveMemorySpace (RAMFB_MMIO_BASE, RAMFB_MMIO_SIZE);
    return Status;
  }

  DEBUG ((
    DEBUG_INFO,
    "Ramfb: Framebuffer at 0x%lx, %lu kB, %lu pages\n",
    (UINT64)FbBase,
    (UINT64)(MaxFbSize / 1024),
    (UINT64)Pages
    ));
  mQemuRamfbMode.FrameBufferSize = MaxFbSize;
  mQemuRamfbMode.FrameBufferBase = FbBase;

  //
  // 800 x 600
  //
  QemuRamfbGraphicsOutputSetMode (&mQemuRamfbGraphicsOutput, 1);

  //
  // ramfb vendor devpath
  //
  VendorDeviceNode.Header.Type    = HARDWARE_DEVICE_PATH;
  VendorDeviceNode.Header.SubType = HW_VENDOR_DP;
  CopyGuid (&VendorDeviceNode.Guid, &gQemuRamfbGuid);
  SetDevicePathNodeLength (
    &VendorDeviceNode.Header,
    sizeof (VENDOR_DEVICE_PATH)
    );

  RamfbDevicePath = AppendDevicePathNode (
                      NULL,
                      (EFI_DEVICE_PATH_PROTOCOL *)&VendorDeviceNode
                      );
  if (RamfbDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeFramebuffer;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mRamfbHandle,
                  &gEfiDevicePathProtocolGuid,
                  RamfbDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Ramfb: install Ramfb Vendor DevicePath failed: %r\n",
      Status
      ));
    goto FreeRamfbDevicePath;
  }

  //
  // gop devpath + protocol
  //
  AcpiDeviceNode.Header.Type    = ACPI_DEVICE_PATH;
  AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
  AcpiDeviceNode.ADR            = ACPI_DISPLAY_ADR (
                                    1,                                      // DeviceIdScheme
                                    0,                                      // HeadId
                                    0,                                      // NonVgaOutput
                                    1,                                      // BiosCanDetect
                                    0,                                      // VendorInfo
                                    ACPI_ADR_DISPLAY_TYPE_EXTERNAL_DIGITAL, // Type
                                    0,                                      // Port
                                    0                                       // Index
                                    );
  SetDevicePathNodeLength (
    &AcpiDeviceNode.Header,
    sizeof (ACPI_ADR_DEVICE_PATH)
    );

  GopDevicePath = AppendDevicePathNode (
                    RamfbDevicePath,
                    (EFI_DEVICE_PATH_PROTOCOL *)&AcpiDeviceNode
                    );
  if (GopDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeRamfbHandle;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mGopHandle,
                  &gEfiDevicePathProtocolGuid,
                  GopDevicePath,
                  &gEfiGraphicsOutputProtocolGuid,
                  &mQemuRamfbGraphicsOutput,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Ramfb: install GOP DevicePath failed: %r\n",
      Status
      ));
    goto FreeGopDevicePath;
  }

  Status = gBS->OpenProtocol (
                  mRamfbHandle,
                  &gEfiDevicePathProtocolGuid,
                  &DevicePath,
                  gImageHandle,
                  mGopHandle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Ramfb: OpenProtocol failed: %r\n", Status));
    goto FreeGopHandle;
  }

  return EFI_SUCCESS;

FreeGopHandle:
  gBS->UninstallMultipleProtocolInterfaces (
         mGopHandle,
         &gEfiDevicePathProtocolGuid,
         GopDevicePath,
         &gEfiGraphicsOutputProtocolGuid,
         &mQemuRamfbGraphicsOutput,
         NULL
         );
FreeGopDevicePath:
  FreePool (GopDevicePath);
FreeRamfbHandle:
  gBS->UninstallMultipleProtocolInterfaces (
         mRamfbHandle,
         &gEfiDevicePathProtocolGuid,
         RamfbDevicePath,
         NULL
         );
FreeRamfbDevicePath:
  FreePool (RamfbDevicePath);
FreeFramebuffer:
  gDS->FreeMemorySpace (mQemuRamfbMode.FrameBufferBase, MaxFbSize);
  gDS->RemoveMemorySpace (RAMFB_MMIO_BASE, RAMFB_MMIO_SIZE);

  return Status;
}
