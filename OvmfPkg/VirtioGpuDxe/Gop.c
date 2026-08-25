









#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include "VirtioGpu.h"
























VOID
ReleaseGopResources (
  IN OUT VGPU_GOP  *VgpuGop,
  IN     BOOLEAN   DisableHead
  )
{
  EFI_STATUS  Status;

  ASSERT (VgpuGop->ResourceId != 0);
  ASSERT (VgpuGop->BackingStore != NULL);






  if (DisableHead) {




    Status = VirtioGpuSetScanout (
               VgpuGop->ParentBus,
               0,
               0,
               0,
               0,
               0,
               0
               );






















    Status = EFI_SUCCESS;




    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      CpuDeadLoop ();
    }
  }




  Status = VirtioGpuResourceDetachBacking (
             VgpuGop->ParentBus,
             VgpuGop->ResourceId
             );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    CpuDeadLoop ();
  }




  VirtioGpuUnmapAndFreeBackingStore (
    VgpuGop->ParentBus,
    VgpuGop->NumberOfPages,
    VgpuGop->BackingStore,
    VgpuGop->BackingStoreMap
    );
  VgpuGop->BackingStore    = NULL;
  VgpuGop->NumberOfPages   = 0;
  VgpuGop->BackingStoreMap = NULL;




  Status = VirtioGpuResourceUnref (
             VgpuGop->ParentBus,
             VgpuGop->ResourceId
             );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    CpuDeadLoop ();
  }

  VgpuGop->ResourceId = 0;
}




typedef struct {
  UINT32    Width;
  UINT32    Height;
} GOP_RESOLUTION;

STATIC CONST GOP_RESOLUTION  mGopResolutions[] = {
  { 640,  480  },
  { 800,  480  },
  { 800,  600  },
  { 832,  624  },
  { 960,  640  },
  { 1024, 600  },
  { 1024, 768  },
  { 1152, 864  },
  { 1152, 870  },
  { 1280, 720  },
  { 1280, 760  },
  { 1280, 768  },
  { 1280, 800  },
  { 1280, 960  },
  { 1280, 1024 },
  { 1360, 768  },
  { 1366, 768  },
  { 1400, 1050 },
  { 1440, 900  },
  { 1600, 900  },
  { 1600, 1200 },
  { 1680, 1050 },
  { 1920, 1080 },
  { 1920, 1200 },
  { 1920, 1440 },
  { 2000, 2000 },
  { 2048, 1536 },
  { 2048, 2048 },
  { 2560, 1440 },
  { 2560, 1600 },
  { 2560, 2048 },
  { 2800, 2100 },
  { 3200, 2400 },
  { 3840, 2160 },
  { 4096, 2160 },
  { 7680, 4320 },
  { 8192, 4320 },
};




#define VGPU_GOP_FROM_GOP(GopPointer) \
          CR (GopPointer, VGPU_GOP, Gop, VGPU_GOP_SIG)

STATIC
VOID
EFIAPI
GopNativeResolution (
  IN  VGPU_GOP  *VgpuGop,
  OUT UINT32    *XRes,
  OUT UINT32    *YRes
  )
{
  volatile VIRTIO_GPU_RESP_DISPLAY_INFO  DisplayInfo;
  EFI_STATUS                             Status;
  UINTN                                  Index;

  Status = VirtioGpuGetDisplayInfo (VgpuGop->ParentBus, &DisplayInfo);
  if (Status != EFI_SUCCESS) {
    return;
  }

  for (Index = 0; Index < VIRTIO_GPU_MAX_SCANOUTS; Index++) {
    if (!DisplayInfo.Pmodes[Index].Enabled ||
        !DisplayInfo.Pmodes[Index].Rectangle.Width ||
        !DisplayInfo.Pmodes[Index].Rectangle.Height)
    {
      continue;
    }

    DEBUG ((
      DEBUG_INFO,
      "%a: #%d: %dx%d\n",
      __func__,
      Index,
      DisplayInfo.Pmodes[Index].Rectangle.Width,
      DisplayInfo.Pmodes[Index].Rectangle.Height
      ));
    if ((*XRes == 0) || (*YRes == 0)) {
      *XRes = DisplayInfo.Pmodes[Index].Rectangle.Width;
      *YRes = DisplayInfo.Pmodes[Index].Rectangle.Height;
    }
  }
}

STATIC
VOID
EFIAPI
GopInitialize (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *This
  )
{
  VGPU_GOP    *VgpuGop;
  EFI_STATUS  Status;
  UINT32      XRes = 0, YRes = 0, Index;

  VgpuGop = VGPU_GOP_FROM_GOP (This);







  VgpuGop->Gop.Mode = &VgpuGop->GopMode;

  VgpuGop->GopMode.MaxMode    = (UINT32)(ARRAY_SIZE (mGopResolutions));
  VgpuGop->GopMode.Info       = &VgpuGop->GopModeInfo;
  VgpuGop->GopMode.SizeOfInfo = sizeof VgpuGop->GopModeInfo;

  VgpuGop->GopModeInfo.PixelFormat = PixelBltOnly;




  GopNativeResolution (VgpuGop, &XRes, &YRes);
  if ((XRes == 0) || (YRes == 0)) {
    return;
  }

  if (PcdGet8 (PcdVideoResolutionSource) == 0) {
    Status = PcdSet32S (PcdVideoHorizontalResolution, XRes);
    ASSERT_RETURN_ERROR (Status);
    Status = PcdSet32S (PcdSetupVideoHorizontalResolution, XRes);
    ASSERT_RETURN_ERROR (Status);
    Status = PcdSet32S (PcdVideoVerticalResolution, YRes);
    ASSERT_RETURN_ERROR (Status);
    Status = PcdSet32S (PcdSetupVideoVerticalResolution, YRes);
    ASSERT_RETURN_ERROR (Status);
    Status = PcdSet8S (PcdVideoResolutionSource, 2);
    ASSERT_RETURN_ERROR (Status);
  }

  VgpuGop->NativeXRes = XRes;
  VgpuGop->NativeYRes = YRes;
  for (Index = 0; Index < ARRAY_SIZE (mGopResolutions); Index++) {
    if ((mGopResolutions[Index].Width == XRes) &&
        (mGopResolutions[Index].Height == YRes))
    {

      return;
    }
  }


  VgpuGop->GopMode.MaxMode++;
}




STATIC
EFI_STATUS
EFIAPI
GopQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *GopModeInfo;

  if ((Info == NULL) ||
      (SizeOfInfo == NULL) ||
      (ModeNumber >= This->Mode->MaxMode))
  {
    return EFI_INVALID_PARAMETER;
  }

  GopModeInfo = AllocateZeroPool (sizeof *GopModeInfo);
  if (GopModeInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (ModeNumber < ARRAY_SIZE (mGopResolutions)) {
    GopModeInfo->HorizontalResolution = mGopResolutions[ModeNumber].Width;
    GopModeInfo->VerticalResolution   = mGopResolutions[ModeNumber].Height;
  } else {
    VGPU_GOP  *VgpuGop = VGPU_GOP_FROM_GOP (This);
    GopModeInfo->HorizontalResolution = VgpuGop->NativeXRes;
    GopModeInfo->VerticalResolution   = VgpuGop->NativeYRes;
  }

  GopModeInfo->PixelFormat       = PixelBltOnly;
  GopModeInfo->PixelsPerScanLine = GopModeInfo->HorizontalResolution;

  *SizeOfInfo = sizeof *GopModeInfo;
  *Info       = GopModeInfo;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
GopSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN  UINT32                        ModeNumber
  )
{
  VGPU_GOP                              *VgpuGop;
  UINT32                                NewResourceId;
  UINTN                                 NewNumberOfBytes;
  UINTN                                 NewNumberOfPages;
  VOID                                  *NewBackingStore;
  EFI_PHYSICAL_ADDRESS                  NewBackingStoreDeviceAddress;
  VOID                                  *NewBackingStoreMap;
  UINTN                                 SizeOfInfo;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *GopModeInfo;

  EFI_STATUS  Status;
  EFI_STATUS  Status2;

  if (!This->Mode) {

    GopInitialize (This);
  }

  Status = GopQueryMode (This, ModeNumber, &SizeOfInfo, &GopModeInfo);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  VgpuGop = VGPU_GOP_FROM_GOP (This);





  if (VgpuGop->ResourceId == 0) {



    NewResourceId = 1;
  } else {







    NewResourceId = 3 - VgpuGop->ResourceId;
  }




  Status = VirtioGpuResourceCreate2d (
             VgpuGop->ParentBus,
             NewResourceId,
             VirtioGpuFormatB8G8R8X8Unorm,
             GopModeInfo->HorizontalResolution,
             GopModeInfo->VerticalResolution
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }





  NewNumberOfBytes = GopModeInfo->HorizontalResolution *
                     GopModeInfo->VerticalResolution * sizeof (UINT32);
  NewNumberOfPages = EFI_SIZE_TO_PAGES (NewNumberOfBytes);
  Status           = VirtioGpuAllocateZeroAndMapBackingStore (
                       VgpuGop->ParentBus,
                       NewNumberOfPages,
                       &NewBackingStore,
                       &NewBackingStoreDeviceAddress,
                       &NewBackingStoreMap
                       );
  if (EFI_ERROR (Status)) {
    goto DestroyHostResource;
  }




  Status = VirtioGpuResourceAttachBacking (
             VgpuGop->ParentBus,
             NewResourceId,
             NewBackingStoreDeviceAddress,
             NewNumberOfPages
             );
  if (EFI_ERROR (Status)) {
    goto UnmapAndFreeBackingStore;
  }




  Status = VirtioGpuSetScanout (
             VgpuGop->ParentBus,
             0,
             0,
             GopModeInfo->HorizontalResolution,
             GopModeInfo->VerticalResolution,
             0,
             NewResourceId
             );
  if (EFI_ERROR (Status)) {
    goto DetachBackingStore;
  }






  if (VgpuGop->ResourceId != 0) {
    Status = VirtioGpuResourceFlush (
               VgpuGop->ParentBus,
               0,
               0,
               GopModeInfo->HorizontalResolution,
               GopModeInfo->VerticalResolution,
               NewResourceId
               );
    if (EFI_ERROR (Status)) {





      Status2 = VirtioGpuSetScanout (
                  VgpuGop->ParentBus,
                  0,
                  0,
                  VgpuGop->GopModeInfo.HorizontalResolution,
                  VgpuGop->GopModeInfo.VerticalResolution,
                  0,
                  VgpuGop->ResourceId
                  );
      ASSERT_EFI_ERROR (Status2);
      if (EFI_ERROR (Status2)) {
        CpuDeadLoop ();
      }

      goto DetachBackingStore;
    }





    ReleaseGopResources (VgpuGop, FALSE );
  }






  ASSERT (VgpuGop->ResourceId == 0);
  ASSERT (VgpuGop->BackingStore == NULL);

  VgpuGop->ResourceId      = NewResourceId;
  VgpuGop->BackingStore    = NewBackingStore;
  VgpuGop->NumberOfPages   = NewNumberOfPages;
  VgpuGop->BackingStoreMap = NewBackingStoreMap;




  VgpuGop->GopMode.Mode = ModeNumber;
  CopyMem (&VgpuGop->GopModeInfo, GopModeInfo, sizeof VgpuGop->GopModeInfo);
  FreePool (GopModeInfo);
  return EFI_SUCCESS;

DetachBackingStore:
  Status2 = VirtioGpuResourceDetachBacking (VgpuGop->ParentBus, NewResourceId);
  ASSERT_EFI_ERROR (Status2);
  if (EFI_ERROR (Status2)) {
    CpuDeadLoop ();
  }

UnmapAndFreeBackingStore:
  VirtioGpuUnmapAndFreeBackingStore (
    VgpuGop->ParentBus,
    NewNumberOfPages,
    NewBackingStore,
    NewBackingStoreMap
    );

DestroyHostResource:
  Status2 = VirtioGpuResourceUnref (VgpuGop->ParentBus, NewResourceId);
  ASSERT_EFI_ERROR (Status2);
  if (EFI_ERROR (Status2)) {
    CpuDeadLoop ();
  }

  FreePool (GopModeInfo);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
GopBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer    OPTIONAL,
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN  UINTN                              SourceX,
  IN  UINTN                              SourceY,
  IN  UINTN                              DestinationX,
  IN  UINTN                              DestinationY,
  IN  UINTN                              Width,
  IN  UINTN                              Height,
  IN  UINTN                              Delta         OPTIONAL
  )
{
  VGPU_GOP    *VgpuGop;
  UINT32      CurrentHorizontal;
  UINT32      CurrentVertical;
  UINTN       SegmentSize;
  UINTN       Y;
  UINTN       ResourceOffset;
  EFI_STATUS  Status;

  VgpuGop           = VGPU_GOP_FROM_GOP (This);
  CurrentHorizontal = VgpuGop->GopModeInfo.HorizontalResolution;
  CurrentVertical   = VgpuGop->GopModeInfo.VerticalResolution;






  SegmentSize = Width * sizeof (UINT32);









  if ((BltOperation == EfiBltVideoToBltBuffer) ||
      (BltOperation == EfiBltBufferToVideo))
  {
    if (Delta == 0) {
      Delta = SegmentSize;
    }
  }





  if ((BltOperation == EfiBltVideoFill) ||
      (BltOperation == EfiBltBufferToVideo) ||
      (BltOperation == EfiBltVideoToVideo))
  {
    if ((DestinationX > CurrentHorizontal) ||
        (Width > CurrentHorizontal - DestinationX) ||
        (DestinationY > CurrentVertical) ||
        (Height > CurrentVertical - DestinationY))
    {
      return EFI_INVALID_PARAMETER;
    }
  }





  if ((BltOperation == EfiBltVideoToBltBuffer) ||
      (BltOperation == EfiBltVideoToVideo))
  {
    if ((SourceX > CurrentHorizontal) ||
        (Width > CurrentHorizontal - SourceX) ||
        (SourceY > CurrentVertical) ||
        (Height > CurrentVertical - SourceY))
    {
      return EFI_INVALID_PARAMETER;
    }
  }





  switch (BltOperation) {
    case EfiBltVideoFill:






      for (Y = 0; Y < Height; ++Y) {
        SetMem32 (
          VgpuGop->BackingStore +
          (DestinationY + Y) * CurrentHorizontal + DestinationX,
          SegmentSize,
          *(UINT32 *)BltBuffer
          );
      }

      break;

    case EfiBltVideoToBltBuffer:







      for (Y = 0; Y < Height; ++Y) {
        CopyMem (
          (UINT8 *)BltBuffer +
          (DestinationY + Y) * Delta + DestinationX * sizeof *BltBuffer,
          VgpuGop->BackingStore +
          (SourceY + Y) * CurrentHorizontal + SourceX,
          SegmentSize
          );
      }

      return EFI_SUCCESS;

    case EfiBltBufferToVideo:







      for (Y = 0; Y < Height; ++Y) {
        CopyMem (
          VgpuGop->BackingStore +
          (DestinationY + Y) * CurrentHorizontal + DestinationX,
          (UINT8 *)BltBuffer +
          (SourceY + Y) * Delta + SourceX * sizeof *BltBuffer,
          SegmentSize
          );
      }

      break;

    case EfiBltVideoToVideo:










      if (SourceY < DestinationY) {
        Y = Height;
        while (Y > 0) {
          --Y;
          CopyMem (
            VgpuGop->BackingStore +
            (DestinationY + Y) * CurrentHorizontal + DestinationX,
            VgpuGop->BackingStore +
            (SourceY + Y) * CurrentHorizontal + SourceX,
            SegmentSize
            );
        }
      } else {
        for (Y = 0; Y < Height; ++Y) {
          CopyMem (
            VgpuGop->BackingStore +
            (DestinationY + Y) * CurrentHorizontal + DestinationX,
            VgpuGop->BackingStore +
            (SourceY + Y) * CurrentHorizontal + SourceX,
            SegmentSize
            );
        }
      }

      break;

    default:
      return EFI_INVALID_PARAMETER;
  }





  ResourceOffset = sizeof (UINT32) * (DestinationY * CurrentHorizontal +
                                      DestinationX);
  Status = VirtioGpuTransferToHost2d (
             VgpuGop->ParentBus,
             (UINT32)DestinationX,
             (UINT32)DestinationY,
             (UINT32)Width,
             (UINT32)Height,
             ResourceOffset,
             VgpuGop->ResourceId
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }




  Status = VirtioGpuResourceFlush (
             VgpuGop->ParentBus,
             (UINT32)DestinationX,
             (UINT32)DestinationY,
             (UINT32)Width,
             (UINT32)Height,
             VgpuGop->ResourceId
             );
  return Status;
}




CONST EFI_GRAPHICS_OUTPUT_PROTOCOL  mGopTemplate = {
  GopQueryMode,
  GopSetMode,
  GopBlt,
  NULL
};
