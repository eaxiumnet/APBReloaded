# APB 2011 Menu Layout — extracted from UIScene serialized docking data

Source: `APBMenus_GameFlowScenes.upk` (UE3 ver547/lic31, LZO-compressed).
Values are authored design-space coordinates from `UIScreenValue_Bounds.Position` (Left/Top/Right/Bottom + ScaleType).

## Scale model (LOCKED)

- **Login_Scene**: design canvas **1152x720**. Foreground dialog = `EVALPOS_PixelViewport` (absolute design px). Backgrounds = `EVALPOS_Percentage*` (fill viewport, overscan). Render = uniform **ScaleToFit** of the 1152x720 canvas into the actual viewport, centered. (AnimBG T=-54/B=774 => symmetric +-54 overscan of 720; verified vs 1286x749 capture: panel left 316*1.040+44 = 373px.)
- **Lobby_Scene**: design canvas **800x600**. Same model. (AnimBG T=-60/B=660 => symmetric +-60 overscan of 600.)
- **UE5 port**: background images in a full-viewport CanvasPanel with anchors from the percentage faces; foreground widgets inside a `ScaleBox(Stretch=ScaleToFit)` wrapping a fixed design-size CanvasPanel with absolute slot offsets = the pixel faces below.


## Login_Scene  (design 1152x720)

### Foreground (PixelViewport, absolute design px)  [37 widgets]

| widget | x | y | w | h | raw L,T,R,B |
|---|--:|--:|--:|--:|---|
| UIImage_APBlogo | 316.0 | 72.0 | 520.0 | 180.0 | 316.0,72.0,836.0,252.0 |
| UIPanel_EULA_MainPanel | 316.0 | 272.0 | 520.0 | 175.0 | 316.0,272.0,836.0,447.0 |
| UIPanel_footer | 316.0 | 487.0 | 520.0 | 40.0 | 316.0,487.0,836.0,527.0 |
| UIImage_credits_under | 316.0 | 487.0 | 258.0 | 18.0 | 316.0,487.0,574.0,505.0 |
| UIImage_credits_under_CREDITS | 578.0 | 509.0 | 258.0 | 18.0 | 578.0,509.0,836.0,527.0 |
| UIImage_credits_under_REPLAY | 578.0 | 487.0 | 258.0 | 18.0 | 578.0,487.0,836.0,505.0 |
| UIImage_credits_under_TOS | 316.0 | 509.0 | 258.0 | 18.0 | 316.0,509.0,574.0,527.0 |
| UILabelButton_AccMgmt | 316.0 | 487.0 | 258.0 | 18.0 | 316.0,487.0,574.0,505.0 |
| UILabelButton_Credits | 578.0 | 509.0 | 258.0 | 18.0 | 578.0,509.0,836.0,527.0 |
| UILabelButton_ReplayVideos | 578.0 | 487.0 | 258.0 | 18.0 | 578.0,487.0,836.0,505.0 |
| UILabelButton_TOS | 316.0 | 509.0 | 258.0 | 18.0 | 316.0,509.0,574.0,527.0 |
| UIImage_dropshadowTOS | 303.0 | 496.0 | 284.0 | 44.0 | 303.0,496.0,587.0,540.0 |
| UIImage_dropshadowREPLAY | 565.0 | 474.0 | 284.0 | 44.0 | 565.0,474.0,849.0,518.0 |
| UIImage_dropshadowCREDITS | 565.0 | 496.0 | 284.0 | 44.0 | 565.0,496.0,849.0,540.0 |
| UIImage_dropshadowACC | 303.0 | 474.0 | 284.0 | 44.0 | 303.0,474.0,587.0,518.0 |
| Check_RememberData | 438.0 | 356.0 | 16.0 | 16.0 | 438.0,356.0,454.0,372.0 |
| EditBox_Password | 439.0 | 384.0 | 352.0 | 19.0 | 439.0,384.0,791.0,403.0 |
| EditBox_UserID | 439.0 | 330.0 | 352.0 | 19.0 | 439.0,330.0,791.0,349.0 |
| UIImage_email_under | 316.0 | 327.0 | 520.0 | 25.0 | 316.0,327.0,836.0,352.0 |
| UIImage_header | 316.0 | 272.0 | 520.0 | 40.0 | 316.0,272.0,836.0,312.0 |
| UIImage_Key_Icon | 320.0 | 276.0 | 32.0 | 32.0 | 320.0,276.0,32.0,32.0 |
| UIImage_main_under | 316.0 | 317.0 | 520.0 | 100.0 | 316.0,317.0,836.0,417.0 |
| UIImage_password_under | 316.0 | 381.0 | 520.0 | 25.0 | 316.0,381.0,836.0,406.0 |
| Label_Instructions | 356.0 | 293.0 | 475.0 | 19.0 | 356.0,293.0,831.0,312.0 |
| Label_Password | 316.0 | 381.0 | 115.0 | 25.0 | 316.0,381.0,431.0,406.0 |
| Label_UserID | 316.0 | 327.0 | 115.0 | 25.0 | 316.0,327.0,431.0,352.0 |
| UILabel | 355.0 | 270.0 | 481.0 | 23.0 | 355.0,270.0,836.0,293.0 |
| UILabel_RememberMeLabel | 458.0 | 356.0 | 128.0 | 16.0 | 458.0,356.0,586.0,372.0 |
| Button_Login | 578.0 | 422.0 | 258.0 | 25.0 | 578.0,422.0,836.0,447.0 |
| UILabelButton_Exit | 316.0 | 422.0 | 258.0 | 25.0 | 316.0,422.0,574.0,447.0 |
| CapsLockWarningPanel | 616.0 | 356.0 | 175.0 | 16.0 | 616.0,356.0,791.0,372.0 |
| CapsLockWarningText | 616.0 | 356.0 | 175.0 | 16.0 | 616.0,356.0,791.0,372.0 |
| UIImage_dropshadowEXIT | 303.0 | 409.0 | 284.0 | 51.0 | 303.0,409.0,587.0,460.0 |
| UIImage_dropshadowLOGIN | 565.0 | 409.0 | 284.0 | 51.0 | 565.0,409.0,849.0,460.0 |
| UIImage_dropshadow | 300.0 | 256.0 | 552.0 | 207.0 | 300.0,256.0,852.0,463.0 |
| UIImage_userIDBG | 439.0 | 330.0 | 352.0 | 19.0 | 439.0,330.0,791.0,349.0 |
| UIImage_passwordBG | 439.0 | 384.0 | 352.0 | 19.0 | 439.0,384.0,791.0,403.0 |

### Background (Percentage/viewport-anchored)  [4 widgets]

| widget | Left | Top | Right | Bottom | scales |
|---|--:|--:|--:|--:|---|
| UIImage | -0.45 | None | 0.75 | 720.0 | {'Left': 'PercentageViewportWS', 'Top': 'PixelViewport', 'Right': 'PercentageViewportWS', 'Bottom': 'PixelViewport'} |
| UIImage | 0.7428 | None | 0.75 | 720.0 | {'Left': 'PercentageViewportWS', 'Top': 'PixelViewport', 'Right': 'PercentageViewportWS', 'Bottom': 'PixelViewport'} |
| UIImage_Anchor | 0.5 | 0.5 | 0.0 | 0.0 | {'Right': 'PercentageOwner', 'Bottom': 'PercentageOwner'} |
| UIImage_AnimBG | -0.075 | -54.0 | 1.075 | 774.0 | {'Left': 'PercentageViewport', 'Top': 'PixelViewport', 'Right': 'PercentageViewport', 'Bottom': 'PixelViewport'} |

## Lobby_Scene  (design 800x600)

### Foreground (PixelViewport, absolute design px)  [66 widgets]

| widget | x | y | w | h | raw L,T,R,B |
|---|--:|--:|--:|--:|---|
| UIImage | 689.0 | 506.0 | 16.0 | 16.0 | 689.0,506.0,16.0,16.0 |
| UIImage_Anchor | 0.5 | 0.5 | 0.0 | 0.0 | 0.5,0.5,0.0,0.0 |
| UIImage_CreateDeleteOverlay | None | None | None | None | None,None,800.0,600.0 |
| UIPanel_Master | None | None | None | None | None,None,800.0,600.0 |
| UIImage_Lobby_Icon | 4.0 | 4.0 | 32.0 | 32.0 | 4.0,4.0,36.0,36.0 |
| UILabel_ChooseACharacter | 41.0 | 23.0 | 759.0 | None | 41.0,23.0,800.0,None |
| UILabel_Lobby_TITLE | 40.0 | 22.0 | 760.0 | 0.0 | 40.0,22.0,800.0,0.0 |
| UIPanel_BusinessModel | 10.0 | 50.0 | 319.0 | 30.0 | 10.0,50.0,329.0,80.0 |
| UIPanel_Character | 336.0 | 300.0 | 128.0 | 25.0 | 336.0,300.0,464.0,325.0 |
| UIPanel_CharacterList | 10.0 | 165.0 | 319.0 | 435.0 | 10.0,165.0,329.0,600.0 |
| UIImage_dropshadowCHARLIST | -6.0 | 165.0 | 351.0 | 398.0 | -6.0,165.0,345.0,563.0 |
| UIPanel_CL_Content | 10.0 | 165.0 | 319.0 | 382.0 | 10.0,165.0,329.0,547.0 |
| cUILabelButton_DeleteChar | 10.0 | 195.0 | 31.0 | 25.0 | 10.0,195.0,41.0,220.0 |
| UIImage_BaseBG | 10.0 | 165.0 | 319.0 | 382.0 | 10.0,165.0,329.0,547.0 |
| UIImage_Characterheader | 10.0 | 165.0 | 319.0 | 25.0 | 10.0,165.0,329.0,190.0 |
| UIImage_Characterheader2 | 10.0 | 195.0 | 319.0 | 25.0 | 10.0,195.0,329.0,220.0 |
| UIImage_Characterheader_SHAD | 10.0 | 190.0 | 319.0 | 5.0 | 10.0,190.0,329.0,195.0 |
| UIImage_headerBG | None | None | None | None | None,None,800.0,40.0 |
| UILabel_CharacterCount | 289.0 | 165.0 | 35.0 | 25.0 | 289.0,165.0,324.0,190.0 |
| UILabel_CharacterHeader | 15.0 | 165.0 | 26.0 | 25.0 | 15.0,165.0,41.0,190.0 |
| UILabelButton_CreateCharacter | 41.0 | 195.0 | 288.0 | 25.0 | 41.0,195.0,329.0,220.0 |
| UILabelButton_DeleteCharacter | 10.0 | 522.0 | 319.0 | 25.0 | 10.0,522.0,329.0,547.0 |
| UILabelButton_Options | 172.0 | 565.0 | 157.0 | 25.0 | 172.0,565.0,329.0,590.0 |
| UILabelButton_Quit | 10.0 | 565.0 | 157.0 | 25.0 | 10.0,565.0,167.0,590.0 |
| UIList_CharacterList | 10.0 | 226.0 | 319.0 | 321.0 | 10.0,226.0,329.0,547.0 |
| UIScrollbar | 309.0 | 226.0 | 20.0 | 321.0 | 309.0,226.0,329.0,547.0 |
| DecrementButton | 309.0 | 226.0 | 20.0 | 32.0 | 309.0,226.0,329.0,258.0 |
| IncrementButton | 309.0 | 515.0 | 20.0 | 32.0 | 309.0,515.0,329.0,547.0 |
| Marker | None | 32.0 | None | 225.0 | None,32.0,None,257.0 |
| UIImage_dropshadowCLOSE | -3.0 | 552.0 | 183.0 | 51.0 | -3.0,552.0,180.0,603.0 |
| UIImage_dropshadowCLOSE | 159.0 | 552.0 | 183.0 | 51.0 | 159.0,552.0,342.0,603.0 |
| UIImage_HEADER_dropshadow | None | 40.0 | None | 5.0 | None,40.0,800.0,5.0 |
| UIImage_FactionIcon | 1.0625 | -10.2399 | 254.9375 | 266.2399 | 1.0625,-10.2399,256.0,256.0 |
| UILabel_CharactersWorldOffline | 336.0 | 302.0 | 128.0 | 18.0 | 336.0,302.0,464.0,320.0 |
| UILabel_LoadingCharacter | 336.0 | 302.0 | 128.0 | 18.0 | 336.0,302.0,464.0,320.0 |
| UIPanel_C_Content | 401.0 | 502.0 | 389.0 | 40.0 | 401.0,502.0,790.0,542.0 |
| UIPanel_Mesh | None | None | None | None | None,None,800.0,600.0 |
| RotationSliders | 329.0 | 40.0 | 471.0 | 462.0 | 329.0,40.0,800.0,502.0 |
| UIImage | 401.0 | 502.0 | 389.0 | 40.0 | 401.0,502.0,790.0,542.0 |
| UIImage_dropshadowCHARINFO | 388.0 | 489.0 | 415.0 | 84.0 | 388.0,489.0,803.0,573.0 |
| UIImage_totalplaytime | 401.0 | 542.0 | 389.0 | 18.0 | 401.0,542.0,790.0,560.0 |
| UILabel_Cash | 446.0 | 521.0 | 244.0 | 21.0 | 446.0,521.0,690.0,542.0 |
| UILabel_CharacterName | 445.0 | 503.0 | 245.0 | 24.0 | 445.0,503.0,690.0,527.0 |
| UILabel_CharacterPlayedTime | 406.0 | 542.0 | 379.0 | 17.0 | 406.0,542.0,785.0,559.0 |
| UILabel_FactionName | 401.0 | 502.0 | 99.99990000000003 | 18.0 | 401.0,502.0,500.9999,18.0 |
| UILabel_TeamName | 589.0002 | 444.9991 | 100.0 | 18.0 | 589.0002,444.9991,689.0002,18.0 |
| UIPanel_AdditionalInfo | 401.0 | 502.0 | 389.0 | 40.0 | 401.0,502.0,790.0,542.0 |
| cUILabelButton_Play | 598.0 | 565.0 | 192.0 | 25.0 | 598.0,565.0,790.0,590.0 |
| UIImage_Threat | 405.0 | 506.0 | 32.0 | 32.0 | 405.0,506.0,437.0,538.0 |
| UIImage_threatbackground | 401.0 | 502.0 | 40.0 | 40.0 | 401.0,502.0,441.0,542.0 |
| UIImage_threatbackground2 | 684.0 | 502.0 | 106.0 | 40.0 | 684.0,502.0,790.0,542.0 |
| UIImage_threatbackground3 | 678.0 | 502.0 | 6.0 | 40.0 | 678.0,502.0,684.0,542.0 |
| UILabel_Rating | 708.0 | 504.0 | 82.0 | 33.0 | 708.0,504.0,790.0,537.0 |
| UILabel_RatingBG | 708.0 | 506.0 | 82.0 | 31.0 | 708.0,506.0,790.0,537.0 |
| UILabelButton_Logout | 401.0 | 565.0 | 192.0 | 25.0 | 401.0,565.0,593.0,590.0 |
| UIImage_dropshadowCLOSE | 388.0 | 552.0 | 218.0 | 51.0 | 388.0,552.0,606.0,603.0 |
| UIImage_dropshadowCLOSE | 585.0 | 552.0 | 218.0 | 51.0 | 585.0,552.0,803.0,603.0 |
| UIImage_RefreshIcon | 336.0 | 174.0 | 128.0 | 128.0 | 336.0,174.0,464.0,302.0 |
| UIImage_BusinessModel_header | 10.0 | 90.0 | 319.0 | 21.0 | 10.0,90.0,329.0,111.0 |
| UIImage_dropshadowBUSMODEL | -6.0 | 50.0 | 351.0 | 120.0 | -6.0,50.0,345.0,170.0 |
| UILabel_Email | 15.0 | 69.0 | 309.0 | 22.0 | 15.0,69.0,324.0,91.0 |
| UILabel_GametimeInfo | 15.0 | 111.0 | 314.0 | 19.0 | 15.0,111.0,329.0,130.0 |
| UILabel_RealTag | 15.0 | 51.0 | 314.0 | 18.0 | 15.0,51.0,329.0,69.0 |
| UILabel_RTWPoints | 15.0 | 51.0 | 309.0 | 18.0 | 15.0,51.0,324.0,69.0 |
| UILabelButton_AccMgmt | 10.0 | 130.0 | 319.0 | 25.0 | 10.0,130.0,329.0,155.0 |
| UILabel_Remaining_title | 15.0 | 90.0 | 314.0 | 21.0 | 15.0,90.0,329.0,111.0 |

### Background (Percentage/viewport-anchored)  [1 widgets]

| widget | Left | Top | Right | Bottom | scales |
|---|--:|--:|--:|--:|---|
| UIImage_AnimBG | -0.1667 | -60.0 | 1.1667 | 660.0 | {'Left': 'PercentageViewport', 'Top': 'PixelViewport', 'Right': 'PercentageViewport', 'Bottom': 'PixelViewport'} |