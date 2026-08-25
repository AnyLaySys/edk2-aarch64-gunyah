








#include "GraphicsConsole.h"




GRAPHICS_CONSOLE_DEV  mGraphicsConsoleDevTemplate = {
  GRAPHICS_CONSOLE_DEV_SIGNATURE,
  (EFI_GRAPHICS_OUTPUT_PROTOCOL *)NULL,
  {
    GraphicsConsoleConOutReset,
    GraphicsConsoleConOutOutputString,
    GraphicsConsoleConOutTestString,
    GraphicsConsoleConOutQueryMode,
    GraphicsConsoleConOutSetMode,
    GraphicsConsoleConOutSetAttribute,
    GraphicsConsoleConOutClearScreen,
    GraphicsConsoleConOutSetCursorPosition,
    GraphicsConsoleConOutEnableCursor,
    (EFI_SIMPLE_TEXT_OUTPUT_MODE *)NULL
  },
  {
    0,
    -1,
    EFI_TEXT_ATTR (EFI_LIGHTGRAY,          EFI_BLACK),
    0,
    0,
    FALSE
  },
  (GRAPHICS_CONSOLE_MODE_DATA *)NULL,
  (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)NULL
};

GRAPHICS_CONSOLE_MODE_DATA  mGraphicsConsoleModeData[] = {
  { 100, 31 },
  { 128, 40 },
  { 160, 42 },
  { 240, 56 },




  { 0,   0  }
};

EFI_HII_DATABASE_PROTOCOL  *mHiiDatabase;
EFI_HII_FONT_PROTOCOL      *mHiiFont;
EFI_HII_HANDLE             mHiiHandle;
VOID                       *mHiiRegistration;

EFI_GUID  mFontPackageListGuid = {
  0xf5f219d3, 0x7006, 0x4648, { 0xac, 0x8d, 0xd6, 0x1d, 0xfb, 0x7b, 0xc6, 0xad }
};

CHAR16  mCrLfString[3] = { CHAR_CARRIAGE_RETURN, CHAR_LINEFEED, CHAR_NULL };

EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mGraphicsEfiColors[16] = {



  { 0x00, 0x00, 0x00, 0x00 },
  { 0x98, 0x00, 0x00, 0x00 },
  { 0x00, 0x98, 0x00, 0x00 },
  { 0x98, 0x98, 0x00, 0x00 },
  { 0x00, 0x00, 0x98, 0x00 },
  { 0x98, 0x00, 0x98, 0x00 },
  { 0x00, 0x98, 0x98, 0x00 },
  { 0x98, 0x98, 0x98, 0x00 },
  { 0x30, 0x30, 0x30, 0x00 },
  { 0xff, 0x00, 0x00, 0x00 },
  { 0x00, 0xff, 0x00, 0x00 },
  { 0xff, 0xff, 0x00, 0x00 },
  { 0x00, 0x00, 0xff, 0x00 },
  { 0xff, 0x00, 0xff, 0x00 },
  { 0x00, 0xff, 0xff, 0x00 },
  { 0xff, 0xff, 0xff, 0x00 }
};

EFI_NARROW_GLYPH  mCursorGlyph = {
  0x0000,
  0x00,
  { 0x00,0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF }
};

CHAR16  SpaceStr[] = { NARROW_CHAR, ' ', 0 };

EFI_DRIVER_BINDING_PROTOCOL  gGraphicsConsoleDriverBinding = {
  GraphicsConsoleControllerDriverSupported,
  GraphicsConsoleControllerDriverStart,
  GraphicsConsoleControllerDriverStop,
  0xa,
  NULL,
  NULL
};
















EFI_STATUS
EFIAPI
GraphicsConsoleControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;

  GraphicsOutput = NULL;



  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)&GraphicsOutput,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }






  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (!EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           Controller,
           &gEfiDevicePathProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
  } else {
    goto Error;
  }




  Status = EfiLocateHiiProtocol ();




Error:
  if (GraphicsOutput != NULL) {
    gBS->CloseProtocol (
           Controller,
           &gEfiGraphicsOutputProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
  }

  return Status;
}

















EFI_STATUS
InitializeGraphicsConsoleTextMode (
  IN UINT32                       HorizontalResolution,
  IN UINT32                       VerticalResolution,
  IN UINT32                       GopModeNumber,
  OUT UINTN                       *TextModeCount,
  OUT GRAPHICS_CONSOLE_MODE_DATA  **TextModeData
  )
{
  UINTN                       Index;
  UINTN                       Count;
  GRAPHICS_CONSOLE_MODE_DATA  *ModeBuffer;
  GRAPHICS_CONSOLE_MODE_DATA  *NewModeBuffer;
  UINTN                       ValidCount;
  UINTN                       ValidIndex;
  UINTN                       MaxColumns;
  UINTN                       MaxRows;

  if ((TextModeCount == NULL) || (TextModeData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Count = sizeof (mGraphicsConsoleModeData) / sizeof (GRAPHICS_CONSOLE_MODE_DATA);





  MaxColumns = HorizontalResolution / EFI_GLYPH_WIDTH;
  MaxRows    = VerticalResolution / EFI_GLYPH_HEIGHT;

  if ((MaxColumns == 0) || (MaxRows == 0)) {
    return EFI_INVALID_PARAMETER;
  }




  mGraphicsConsoleModeData[Count - 1].Columns = MaxColumns;
  mGraphicsConsoleModeData[Count - 1].Rows    = MaxRows;




  ModeBuffer = mGraphicsConsoleModeData;






  NewModeBuffer = AllocateZeroPool (sizeof (GRAPHICS_CONSOLE_MODE_DATA) * (Count + 2));
  if (NewModeBuffer == NULL) {
    ASSERT (NewModeBuffer != NULL);
    return EFI_OUT_OF_RESOURCES;
  }

  ValidCount = 0;

  if ((MaxColumns >= 80) && (MaxRows >= 25)) {
    NewModeBuffer[ValidCount].Columns       = 80;
    NewModeBuffer[ValidCount].Rows          = 25;
    NewModeBuffer[ValidCount].GopWidth      = HorizontalResolution;
    NewModeBuffer[ValidCount].GopHeight     = VerticalResolution;
    NewModeBuffer[ValidCount].GopModeNumber = GopModeNumber;
    NewModeBuffer[ValidCount].DeltaX        = (HorizontalResolution - (NewModeBuffer[ValidCount].Columns * EFI_GLYPH_WIDTH)) >> 1;
    NewModeBuffer[ValidCount].DeltaY        = (VerticalResolution - (NewModeBuffer[ValidCount].Rows * EFI_GLYPH_HEIGHT)) >> 1;
    ValidCount++;
  }

  if ((MaxColumns >= 80) && (MaxRows >= 50)) {
    NewModeBuffer[ValidCount].Columns       = 80;
    NewModeBuffer[ValidCount].Rows          = 50;
    NewModeBuffer[ValidCount].GopWidth      = HorizontalResolution;
    NewModeBuffer[ValidCount].GopHeight     = VerticalResolution;
    NewModeBuffer[ValidCount].GopModeNumber = GopModeNumber;
    NewModeBuffer[ValidCount].DeltaX        = (HorizontalResolution - (80 * EFI_GLYPH_WIDTH)) >> 1;
    NewModeBuffer[ValidCount].DeltaY        = (VerticalResolution - (50 * EFI_GLYPH_HEIGHT)) >> 1;
    ValidCount++;
  }




  for (Index = 0; Index < Count; Index++) {
    if ((ModeBuffer[Index].Columns == 0) || (ModeBuffer[Index].Rows == 0) ||
        (ModeBuffer[Index].Columns > MaxColumns) || (ModeBuffer[Index].Rows > MaxRows))
    {



      continue;
    }

    for (ValidIndex = 0; ValidIndex < ValidCount; ValidIndex++) {
      if ((ModeBuffer[Index].Columns == NewModeBuffer[ValidIndex].Columns) &&
          (ModeBuffer[Index].Rows == NewModeBuffer[ValidIndex].Rows))
      {



        break;
      }
    }

    if (ValidIndex == ValidCount) {
      NewModeBuffer[ValidCount].Columns       = ModeBuffer[Index].Columns;
      NewModeBuffer[ValidCount].Rows          = ModeBuffer[Index].Rows;
      NewModeBuffer[ValidCount].GopWidth      = HorizontalResolution;
      NewModeBuffer[ValidCount].GopHeight     = VerticalResolution;
      NewModeBuffer[ValidCount].GopModeNumber = GopModeNumber;
      NewModeBuffer[ValidCount].DeltaX        = (HorizontalResolution - (NewModeBuffer[ValidCount].Columns * EFI_GLYPH_WIDTH)) >> 1;
      NewModeBuffer[ValidCount].DeltaY        = (VerticalResolution - (NewModeBuffer[ValidCount].Rows * EFI_GLYPH_HEIGHT)) >> 1;
      ValidCount++;
    }
  }

  DEBUG_CODE_BEGIN ();
  for (Index = 0; Index < ValidCount; Index++) {
    DEBUG ((
      DEBUG_INFO,
      "Graphics - Mode %d, Column = %d, Row = %d\n",
      Index,
      NewModeBuffer[Index].Columns,
      NewModeBuffer[Index].Rows
      ));
  }

  DEBUG_CODE_END ();




  *TextModeCount = ValidCount;
  *TextModeData  = NewModeBuffer;
  return EFI_SUCCESS;
}














EFI_STATUS
EFIAPI
GraphicsConsoleControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                            Status;
  GRAPHICS_CONSOLE_DEV                  *Private;
  UINT32                                HorizontalResolution;
  UINT32                                VerticalResolution;
  UINT32                                ModeIndex;
  UINTN                                 MaxMode;
  UINT32                                ModeNumber;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE     *Mode;
  UINTN                                 SizeOfInfo;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
  INT32                                 PreferMode;
  INT32                                 Index;
  UINTN                                 Column;
  UINTN                                 Row;
  UINTN                                 DefaultColumn;
  UINTN                                 DefaultRow;

  ModeNumber = 0;




  Private = AllocateCopyPool (
              sizeof (GRAPHICS_CONSOLE_DEV),
              &mGraphicsConsoleDevTemplate
              );
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->SimpleTextOutput.Mode = &(Private->SimpleTextOutputMode);

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)&Private->GraphicsOutput,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  HorizontalResolution = PcdGet32 (PcdVideoHorizontalResolution);
  VerticalResolution   = PcdGet32 (PcdVideoVerticalResolution);

  if (Private->GraphicsOutput != NULL) {





    if ((HorizontalResolution == 0x0) || (VerticalResolution == 0x0)) {



      MaxMode = Private->GraphicsOutput->Mode->MaxMode;

      for (ModeIndex = 0; (UINTN)ModeIndex < MaxMode; ModeIndex++) {
        Status = Private->GraphicsOutput->QueryMode (
                                            Private->GraphicsOutput,
                                            ModeIndex,
                                            &SizeOfInfo,
                                            &Info
                                            );
        if (!EFI_ERROR (Status)) {
          if ((Info->HorizontalResolution > HorizontalResolution) ||
              ((Info->HorizontalResolution == HorizontalResolution) && (Info->VerticalResolution > VerticalResolution)))
          {
            HorizontalResolution = Info->HorizontalResolution;
            VerticalResolution   = Info->VerticalResolution;
            ModeNumber           = ModeIndex;
          }

          FreePool (Info);
        }
      }

      if ((HorizontalResolution == 0x0) || (VerticalResolution == 0x0)) {
        Status = EFI_UNSUPPORTED;
        goto Error;
      }
    } else {



      Status = CheckModeSupported (
                 Private->GraphicsOutput,
                 HorizontalResolution,
                 VerticalResolution,
                 &ModeNumber
                 );
      if (EFI_ERROR (Status)) {



        HorizontalResolution = 800;
        VerticalResolution   = 600;
        Status               = CheckModeSupported (
                                 Private->GraphicsOutput,
                                 HorizontalResolution,
                                 VerticalResolution,
                                 &ModeNumber
                                 );
        Mode = Private->GraphicsOutput->Mode;
        if (EFI_ERROR (Status) && (Mode->MaxMode != 0)) {



          HorizontalResolution = Mode->Info->HorizontalResolution;
          VerticalResolution   = Mode->Info->VerticalResolution;
          ModeNumber           = Mode->Mode;
        }
      }
    }

    if (EFI_ERROR (Status) || (ModeNumber != Private->GraphicsOutput->Mode->Mode)) {




      Status = Private->GraphicsOutput->SetMode (Private->GraphicsOutput, ModeNumber);
      if (EFI_ERROR (Status)) {



        goto Error;
      }
    }
  }

  DEBUG ((DEBUG_INFO, "GraphicsConsole video resolution %d x %d\n", HorizontalResolution, VerticalResolution));




  Status = InitializeGraphicsConsoleTextMode (
             HorizontalResolution,
             VerticalResolution,
             ModeNumber,
             &MaxMode,
             &Private->ModeData
             );

  if (EFI_ERROR (Status)) {
    goto Error;
  }




  Private->SimpleTextOutputMode.MaxMode = (INT32)MaxMode;




  PreferMode    = -1;
  DefaultColumn = PcdGet32 (PcdConOutColumn);
  DefaultRow    = PcdGet32 (PcdConOutRow);
  Column        = 0;
  Row           = 0;
  for (Index = 0; Index < (INT32)MaxMode; Index++) {
    if ((DefaultColumn != 0) && (DefaultRow != 0)) {
      if ((Private->ModeData[Index].Columns == DefaultColumn) &&
          (Private->ModeData[Index].Rows == DefaultRow))
      {
        PreferMode = Index;
        break;
      }
    } else {
      if ((Private->ModeData[Index].Columns > Column) &&
          (Private->ModeData[Index].Rows > Row))
      {
        Column     = Private->ModeData[Index].Columns;
        Row        = Private->ModeData[Index].Rows;
        PreferMode = Index;
      }
    }
  }

  Private->SimpleTextOutput.Mode->Mode = (INT32)PreferMode;
  DEBUG ((DEBUG_INFO, "Graphics Console Started, Mode: %d\n", PreferMode));




  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEfiSimpleTextOutProtocolGuid,
                  &Private->SimpleTextOutput,
                  NULL
                  );

Error:
  if (EFI_ERROR (Status)) {



    if (Private->GraphicsOutput != NULL) {
      gBS->CloseProtocol (
             Controller,
             &gEfiGraphicsOutputProtocolGuid,
             This->DriverBindingHandle,
             Controller
             );
    }

    if (Private->LineBuffer != NULL) {
      FreePool (Private->LineBuffer);
    }

    if (Private->ModeData != NULL) {
      FreePool (Private->ModeData);
    }




    FreePool (Private);
  }

  return Status;
}

















EFI_STATUS
EFIAPI
GraphicsConsoleControllerDriverStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS                       Status;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *SimpleTextOutput;
  GRAPHICS_CONSOLE_DEV             *Private;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiSimpleTextOutProtocolGuid,
                  (VOID **)&SimpleTextOutput,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_STARTED;
  }

  Private = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (SimpleTextOutput);

  Status = gBS->UninstallProtocolInterface (
                  Controller,
                  &gEfiSimpleTextOutProtocolGuid,
                  &Private->SimpleTextOutput
                  );

  if (!EFI_ERROR (Status)) {



    if (Private->GraphicsOutput != NULL) {
      gBS->CloseProtocol (
             Controller,
             &gEfiGraphicsOutputProtocolGuid,
             This->DriverBindingHandle,
             Controller
             );
    }

    if (Private->LineBuffer != NULL) {
      FreePool (Private->LineBuffer);
    }

    if (Private->ModeData != NULL) {
      FreePool (Private->ModeData);
    }




    FreePool (Private);
  }

  return Status;
}




















EFI_STATUS
CheckModeSupported (
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput,
  IN  UINT32                    HorizontalResolution,
  IN  UINT32                    VerticalResolution,
  OUT UINT32                    *CurrentModeNumber
  )
{
  UINT32                                ModeNumber;
  EFI_STATUS                            Status;
  UINTN                                 SizeOfInfo;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
  UINT32                                MaxMode;

  Status  = EFI_SUCCESS;
  MaxMode = GraphicsOutput->Mode->MaxMode;

  for (ModeNumber = 0; ModeNumber < MaxMode; ModeNumber++) {
    Status = GraphicsOutput->QueryMode (
                               GraphicsOutput,
                               ModeNumber,
                               &SizeOfInfo,
                               &Info
                               );
    if (!EFI_ERROR (Status)) {
      if ((Info->HorizontalResolution == HorizontalResolution) &&
          (Info->VerticalResolution == VerticalResolution))
      {
        if ((GraphicsOutput->Mode->Info->HorizontalResolution == HorizontalResolution) &&
            (GraphicsOutput->Mode->Info->VerticalResolution == VerticalResolution))
        {



          FreePool (Info);
          break;
        } else {
          Status = GraphicsOutput->SetMode (GraphicsOutput, ModeNumber);
          if (!EFI_ERROR (Status)) {
            FreePool (Info);
            break;
          }
        }
      }

      FreePool (Info);
    }
  }

  if (ModeNumber == GraphicsOutput->Mode->MaxMode) {
    Status = EFI_UNSUPPORTED;
  }

  *CurrentModeNumber = ModeNumber;
  return Status;
}










EFI_STATUS
EfiLocateHiiProtocol (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&mHiiDatabase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiHiiFontProtocolGuid, NULL, (VOID **)&mHiiFont);
  return Status;
}























EFI_STATUS
EFIAPI
GraphicsConsoleConOutReset (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  BOOLEAN                          ExtendedVerification
  )
{
  EFI_STATUS  Status;

  Status = This->SetMode (This, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = This->SetAttribute (This, EFI_TEXT_ATTR (This->Mode->Attribute & 0x0F, EFI_BACKGROUND_BLACK));
  return Status;
}























EFI_STATUS
EFIAPI
GraphicsConsoleConOutOutputString (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  CHAR16                           *WString
  )
{
  GRAPHICS_CONSOLE_DEV           *Private;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  INTN                           Mode;
  UINTN                          MaxColumn;
  UINTN                          MaxRow;
  UINTN                          Width;
  UINTN                          Height;
  UINTN                          Delta;
  EFI_STATUS                     Status;
  BOOLEAN                        Warning;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Foreground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  UINTN                          DeltaX;
  UINTN                          DeltaY;
  UINTN                          Count;
  UINTN                          Index;
  INT32                          OriginAttribute;
  EFI_TPL                        OldTpl;

  if (This->Mode->Mode == -1) {



    return EFI_UNSUPPORTED;
  }

  Status = EFI_SUCCESS;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);



  Mode           = This->Mode->Mode;
  Private        = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);
  GraphicsOutput = Private->GraphicsOutput;

  MaxColumn = Private->ModeData[Mode].Columns;
  MaxRow    = Private->ModeData[Mode].Rows;
  DeltaX    = (UINTN)Private->ModeData[Mode].DeltaX;
  DeltaY    = (UINTN)Private->ModeData[Mode].DeltaY;
  Width     = MaxColumn * EFI_GLYPH_WIDTH;
  Height    = (MaxRow - 1) * EFI_GLYPH_HEIGHT;
  Delta     = Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);




  GetTextColors (This, &Foreground, &Background);

  FlushCursor (This);

  Warning = FALSE;




  OriginAttribute = This->Mode->Attribute;

  while (*WString != L'\0') {
    if (*WString == CHAR_BACKSPACE) {




      if ((This->Mode->CursorColumn == 0) && (This->Mode->CursorRow > 0)) {
        This->Mode->CursorRow--;
        This->Mode->CursorColumn = (INT32)(MaxColumn - 1);
        This->OutputString (This, SpaceStr);
        FlushCursor (This);
        This->Mode->CursorRow--;
        This->Mode->CursorColumn = (INT32)(MaxColumn - 1);
      } else if (This->Mode->CursorColumn > 0) {




        This->Mode->CursorColumn--;
        This->OutputString (This, SpaceStr);
        FlushCursor (This);
        This->Mode->CursorColumn--;
      }

      WString++;
    } else if (*WString == CHAR_LINEFEED) {





      if (This->Mode->CursorRow == (INT32)(MaxRow - 1)) {
        if (GraphicsOutput != NULL) {



          GraphicsOutput->Blt (
                            GraphicsOutput,
                            NULL,
                            EfiBltVideoToVideo,
                            DeltaX,
                            DeltaY + EFI_GLYPH_HEIGHT,
                            DeltaX,
                            DeltaY,
                            Width,
                            Height,
                            Delta
                            );




          GraphicsOutput->Blt (
                            GraphicsOutput,
                            &Background,
                            EfiBltVideoFill,
                            0,
                            0,
                            DeltaX,
                            DeltaY + Height,
                            Width,
                            EFI_GLYPH_HEIGHT,
                            Delta
                            );
        }
      } else {
        This->Mode->CursorRow++;
      }

      WString++;
    } else if (*WString == CHAR_CARRIAGE_RETURN) {



      This->Mode->CursorColumn = 0;
      WString++;
    } else if (*WString == WIDE_CHAR) {
      This->Mode->Attribute |= EFI_WIDE_ATTRIBUTE;
      WString++;
    } else if (*WString == NARROW_CHAR) {
      This->Mode->Attribute &= (~(UINT32)EFI_WIDE_ATTRIBUTE);
      WString++;
    } else {













      for (Count = 0, Index = 0; (This->Mode->CursorColumn + Index) < MaxColumn; Count++, Index++) {
        if ((WString[Count] == CHAR_NULL) ||
            (WString[Count] == CHAR_BACKSPACE) ||
            (WString[Count] == CHAR_LINEFEED) ||
            (WString[Count] == CHAR_CARRIAGE_RETURN) ||
            (WString[Count] == WIDE_CHAR) ||
            (WString[Count] == NARROW_CHAR))
        {
          break;
        }




        if ((This->Mode->Attribute & EFI_WIDE_ATTRIBUTE) != 0) {



          Index++;





          if ((This->Mode->CursorColumn + Index + 1) > MaxColumn) {
            Index++;
            break;
          }
        }
      }

      Status = DrawUnicodeWeightAtCursorN (This, WString, Count);
      if (EFI_ERROR (Status)) {
        Warning = TRUE;
      }




      WString                  += Count;
      This->Mode->CursorColumn += (INT32)Index;
      if (This->Mode->CursorColumn > (INT32)MaxColumn) {
        This->Mode->CursorColumn -= 2;
        This->OutputString (This, SpaceStr);
      }

      if (This->Mode->CursorColumn >= (INT32)MaxColumn) {
        FlushCursor (This);
        This->OutputString (This, mCrLfString);
        FlushCursor (This);
      }
    }
  }

  This->Mode->Attribute = OriginAttribute;

  FlushCursor (This);

  if (Warning) {
    Status = EFI_WARN_UNKNOWN_GLYPH;
  }

  gBS->RestoreTPL (OldTpl);
  return Status;
}




















EFI_STATUS
EFIAPI
GraphicsConsoleConOutTestString (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  CHAR16                           *WString
  )
{
  EFI_STATUS  Status;
  UINT16      Count;

  EFI_IMAGE_OUTPUT  *Blt;

  Blt   = NULL;
  Count = 0;

  while (WString[Count] != 0) {




    if ((WString[Count] == WIDE_CHAR) || (WString[Count] == NARROW_CHAR)) {
      continue;
    }

    Status = mHiiFont->GetGlyph (
                         mHiiFont,
                         WString[Count],
                         NULL,
                         &Blt,
                         NULL
                         );
    if (Blt != NULL) {
      FreePool (Blt);
      Blt = NULL;
    }

    Count++;

    if (EFI_ERROR (Status)) {
      return EFI_UNSUPPORTED;
    }
  }

  return EFI_SUCCESS;
}


















EFI_STATUS
EFIAPI
GraphicsConsoleConOutQueryMode (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            ModeNumber,
  OUT UINTN                            *Columns,
  OUT UINTN                            *Rows
  )
{
  GRAPHICS_CONSOLE_DEV  *Private;
  EFI_STATUS            Status;
  EFI_TPL               OldTpl;

  if (ModeNumber >= (UINTN)This->Mode->MaxMode) {
    return EFI_UNSUPPORTED;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  Status = EFI_SUCCESS;

  Private = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);

  *Columns = Private->ModeData[ModeNumber].Columns;
  *Rows    = Private->ModeData[ModeNumber].Rows;

  if ((*Columns <= 0) || (*Rows <= 0)) {
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

Done:
  gBS->RestoreTPL (OldTpl);
  return Status;
}
















EFI_STATUS
EFIAPI
GraphicsConsoleConOutSetMode (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            ModeNumber
  )
{
  EFI_STATUS                     Status;
  GRAPHICS_CONSOLE_DEV           *Private;
  GRAPHICS_CONSOLE_MODE_DATA     *ModeData;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *NewLineBuffer;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  EFI_TPL                        OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Private        = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);
  GraphicsOutput = Private->GraphicsOutput;




  if (ModeNumber >= (UINTN)This->Mode->MaxMode) {
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  ModeData = &(Private->ModeData[ModeNumber]);

  if ((ModeData->Columns <= 0) && (ModeData->Rows <= 0)) {
    Status = EFI_UNSUPPORTED;
    goto Done;
  }




  if (Private->LineBuffer != NULL) {



    if ((INT32)ModeNumber == This->Mode->Mode) {



      This->ClearScreen (This);
      Status = EFI_SUCCESS;
      goto Done;
    }





    FlushCursor (This);

    FreePool (Private->LineBuffer);
  }




  NewLineBuffer = AllocatePool (sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * ModeData->Columns * EFI_GLYPH_WIDTH * EFI_GLYPH_HEIGHT);

  if (NewLineBuffer == NULL) {




    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }




  Private->LineBuffer = NewLineBuffer;

  if (GraphicsOutput != NULL) {
    if (ModeData->GopModeNumber != GraphicsOutput->Mode->Mode) {



      Status = GraphicsOutput->SetMode (GraphicsOutput, ModeData->GopModeNumber);
      if (EFI_ERROR (Status)) {



        goto Done;
      }
    } else {



      Status = GraphicsOutput->Blt (
                                 GraphicsOutput,
                                 &mGraphicsEfiColors[0],
                                 EfiBltVideoFill,
                                 0,
                                 0,
                                 0,
                                 0,
                                 ModeData->GopWidth,
                                 ModeData->GopHeight,
                                 0
                                 );
    }
  }




  This->Mode->Mode = (INT32)ModeNumber;




  This->Mode->CursorColumn = 0;
  This->Mode->CursorRow    = 0;

  FlushCursor (This);

  Status = EFI_SUCCESS;

Done:
  gBS->RestoreTPL (OldTpl);
  return Status;
}

















EFI_STATUS
EFIAPI
GraphicsConsoleConOutSetAttribute (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            Attribute
  )
{
  EFI_TPL  OldTpl;

  if ((Attribute | 0x7F) != 0x7F) {
    return EFI_UNSUPPORTED;
  }

  if ((INT32)Attribute == This->Mode->Attribute) {
    return EFI_SUCCESS;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  FlushCursor (This);

  This->Mode->Attribute = (INT32)Attribute;

  FlushCursor (This);

  gBS->RestoreTPL (OldTpl);

  return EFI_SUCCESS;
}














EFI_STATUS
EFIAPI
GraphicsConsoleConOutClearScreen (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This
  )
{
  EFI_STATUS                     Status;
  GRAPHICS_CONSOLE_DEV           *Private;
  GRAPHICS_CONSOLE_MODE_DATA     *ModeData;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Foreground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  EFI_TPL                        OldTpl;

  if (This->Mode->Mode == -1) {



    return EFI_UNSUPPORTED;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Private        = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);
  GraphicsOutput = Private->GraphicsOutput;
  ModeData       = &(Private->ModeData[This->Mode->Mode]);

  GetTextColors (This, &Foreground, &Background);
  if (GraphicsOutput != NULL) {
    Status = GraphicsOutput->Blt (
                               GraphicsOutput,
                               &Background,
                               EfiBltVideoFill,
                               0,
                               0,
                               0,
                               0,
                               ModeData->GopWidth,
                               ModeData->GopHeight,
                               0
                               );
  } else {
    Status = EFI_UNSUPPORTED;
  }

  This->Mode->CursorColumn = 0;
  This->Mode->CursorRow    = 0;

  FlushCursor (This);

  gBS->RestoreTPL (OldTpl);

  return Status;
}




















EFI_STATUS
EFIAPI
GraphicsConsoleConOutSetCursorPosition (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            Column,
  IN  UINTN                            Row
  )
{
  GRAPHICS_CONSOLE_DEV        *Private;
  GRAPHICS_CONSOLE_MODE_DATA  *ModeData;
  EFI_STATUS                  Status;
  EFI_TPL                     OldTpl;

  if (This->Mode->Mode == -1) {



    return EFI_UNSUPPORTED;
  }

  Status = EFI_SUCCESS;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Private  = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);
  ModeData = &(Private->ModeData[This->Mode->Mode]);

  if ((Column >= ModeData->Columns) || (Row >= ModeData->Rows)) {
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  if ((This->Mode->CursorColumn == (INT32)Column) && (This->Mode->CursorRow == (INT32)Row)) {
    Status = EFI_SUCCESS;
    goto Done;
  }

  FlushCursor (This);

  This->Mode->CursorColumn = (INT32)Column;
  This->Mode->CursorRow    = (INT32)Row;

  FlushCursor (This);

Done:
  gBS->RestoreTPL (OldTpl);

  return Status;
}















EFI_STATUS
EFIAPI
GraphicsConsoleConOutEnableCursor (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  BOOLEAN                          Visible
  )
{
  EFI_TPL  OldTpl;

  if (This->Mode->Mode == -1) {



    return EFI_UNSUPPORTED;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  FlushCursor (This);

  This->Mode->CursorVisible = Visible;

  FlushCursor (This);

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;
}











EFI_STATUS
GetTextColors (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Foreground,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Background
  )
{
  INTN  Attribute;

  Attribute = This->Mode->Attribute & 0x7F;

  *Foreground = mGraphicsEfiColors[Attribute & 0x0f];
  *Background = mGraphicsEfiColors[Attribute >> 4];

  return EFI_SUCCESS;
}














EFI_STATUS
DrawUnicodeWeightAtCursorN (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  CHAR16                           *UnicodeWeight,
  IN  UINTN                            Count
  )
{
  EFI_STATUS             Status;
  GRAPHICS_CONSOLE_DEV   *Private;
  EFI_IMAGE_OUTPUT       *Blt;
  EFI_STRING             String;
  EFI_FONT_DISPLAY_INFO  *FontInfo;

  Private = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);
  Blt     = (EFI_IMAGE_OUTPUT *)AllocateZeroPool (sizeof (EFI_IMAGE_OUTPUT));
  if (Blt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Blt->Width  = (UINT16)(Private->ModeData[This->Mode->Mode].GopWidth);
  Blt->Height = (UINT16)(Private->ModeData[This->Mode->Mode].GopHeight);

  String = AllocateCopyPool ((Count + 1) * sizeof (CHAR16), UnicodeWeight);
  if (String == NULL) {
    FreePool (Blt);
    return EFI_OUT_OF_RESOURCES;
  }




  *(String + Count) = L'\0';

  FontInfo = (EFI_FONT_DISPLAY_INFO *)AllocateZeroPool (sizeof (EFI_FONT_DISPLAY_INFO));
  if (FontInfo == NULL) {
    FreePool (Blt);
    FreePool (String);
    return EFI_OUT_OF_RESOURCES;
  }




  GetTextColors (This, &FontInfo->ForegroundColor, &FontInfo->BackgroundColor);

  if (Private->GraphicsOutput != NULL) {



    Blt->Image.Screen = Private->GraphicsOutput;

    Status = mHiiFont->StringToImage (
                         mHiiFont,
                         EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_DIRECT_TO_SCREEN | EFI_HII_IGNORE_LINE_BREAK,
                         String,
                         FontInfo,
                         &Blt,
                         This->Mode->CursorColumn * EFI_GLYPH_WIDTH + Private->ModeData[This->Mode->Mode].DeltaX,
                         This->Mode->CursorRow * EFI_GLYPH_HEIGHT + Private->ModeData[This->Mode->Mode].DeltaY,
                         NULL,
                         NULL,
                         NULL
                         );
  } else {
    Status = EFI_UNSUPPORTED;
  }

  if (Blt != NULL) {
    FreePool (Blt);
  }

  if (String != NULL) {
    FreePool (String);
  }

  if (FontInfo != NULL) {
    FreePool (FontInfo);
  }

  return Status;
}














EFI_STATUS
FlushCursor (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This
  )
{
  GRAPHICS_CONSOLE_DEV                 *Private;
  EFI_SIMPLE_TEXT_OUTPUT_MODE          *CurrentMode;
  INTN                                 GlyphX;
  INTN                                 GlyphY;
  EFI_GRAPHICS_OUTPUT_PROTOCOL         *GraphicsOutput;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  Foreground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  Background;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  BltChar[EFI_GLYPH_HEIGHT][EFI_GLYPH_WIDTH];
  UINTN                                PosX;
  UINTN                                PosY;

  CurrentMode = This->Mode;

  if (!CurrentMode->CursorVisible) {
    return EFI_SUCCESS;
  }

  Private        = GRAPHICS_CONSOLE_CON_OUT_DEV_FROM_THIS (This);
  GraphicsOutput = Private->GraphicsOutput;







  GlyphX = (CurrentMode->CursorColumn * EFI_GLYPH_WIDTH) + Private->ModeData[CurrentMode->Mode].DeltaX;
  GlyphY = (CurrentMode->CursorRow * EFI_GLYPH_HEIGHT) + Private->ModeData[CurrentMode->Mode].DeltaY;
  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt (
                      GraphicsOutput,
                      (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)BltChar,
                      EfiBltVideoToBltBuffer,
                      GlyphX,
                      GlyphY,
                      0,
                      0,
                      EFI_GLYPH_WIDTH,
                      EFI_GLYPH_HEIGHT,
                      EFI_GLYPH_WIDTH * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                      );
  }

  GetTextColors (This, &Foreground.Pixel, &Background.Pixel);




  for (PosY = 0; PosY < EFI_GLYPH_HEIGHT; PosY++) {
    for (PosX = 0; PosX < EFI_GLYPH_WIDTH; PosX++) {
      if ((mCursorGlyph.GlyphCol1[PosY] & (BIT0 << PosX)) != 0) {
        BltChar[PosY][EFI_GLYPH_WIDTH - PosX - 1].Raw ^= Foreground.Raw;
      }
    }
  }

  if (GraphicsOutput != NULL) {
    GraphicsOutput->Blt (
                      GraphicsOutput,
                      (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)BltChar,
                      EfiBltBufferToVideo,
                      0,
                      0,
                      GlyphX,
                      GlyphY,
                      EFI_GLYPH_WIDTH,
                      EFI_GLYPH_HEIGHT,
                      EFI_GLYPH_WIDTH * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                      );
  }

  return EFI_SUCCESS;
}









VOID
EFIAPI
RegisterFontPackage (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS                       Status;
  EFI_HII_SIMPLE_FONT_PACKAGE_HDR  *SimplifiedFont;
  UINT32                           PackageLength;
  UINT8                            *Package;
  UINT8                            *Location;
  EFI_HII_DATABASE_PROTOCOL        *HiiDatabase;




  Status = gBS->LocateProtocol (
                  &gEfiHiiDatabaseProtocolGuid,
                  NULL,
                  (VOID **)&HiiDatabase
                  );
  if (EFI_ERROR (Status)) {
    return;
  }


















  PackageLength = sizeof (EFI_HII_SIMPLE_FONT_PACKAGE_HDR) + mNarrowFontSize + 4;
  Package       = AllocateZeroPool (PackageLength);
  if (Package == NULL) {
    ASSERT (Package != NULL);
    return;
  }

  WriteUnaligned32 ((UINT32 *)Package, PackageLength);
  SimplifiedFont                       = (EFI_HII_SIMPLE_FONT_PACKAGE_HDR *)(Package + 4);
  SimplifiedFont->Header.Length        = (UINT32)(PackageLength - 4);
  SimplifiedFont->Header.Type          = EFI_HII_PACKAGE_SIMPLE_FONTS;
  SimplifiedFont->NumberOfNarrowGlyphs = (UINT16)(mNarrowFontSize / sizeof (EFI_NARROW_GLYPH));

  Location = (UINT8 *)(&SimplifiedFont->NumberOfWideGlyphs + 1);
  CopyMem (Location, gUsStdNarrowGlyphData, mNarrowFontSize);




  mHiiHandle = HiiAddPackages (
                 &mFontPackageListGuid,
                 NULL,
                 Package,
                 NULL
                 );
  ASSERT (mHiiHandle != NULL);
  FreePool (Package);
}











EFI_STATUS
EFIAPI
InitializeGraphicsConsole (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;




  EfiCreateProtocolNotifyEvent (
    &gEfiHiiDatabaseProtocolGuid,
    TPL_CALLBACK,
    RegisterFontPackage,
    NULL,
    &mHiiRegistration
    );




  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gGraphicsConsoleDriverBinding,
             ImageHandle,
             &gGraphicsConsoleComponentName,
             &gGraphicsConsoleComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
