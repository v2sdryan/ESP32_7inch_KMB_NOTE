# ESP32_7inch_KMB_NOTE Windows／macOS 詳細安裝手冊

本手冊畀第一次接觸 ESP32 嘅使用者跟住做。你唔需要先識 Arduino，但要有一條可以傳輸資料嘅 USB 線、穩定網絡，同埋大約 30–60 分鐘完成第一次工具下載及燒錄。

## 1. 支援硬件

本版本已針對以下組合編譯及實機驗證：

| 項目 | 規格 |
| --- | --- |
| MCU | ESP32-S3，dual core，最高 240 MHz |
| PSRAM | 8 MB OPI PSRAM |
| 實體 flash | 16 MB internal flash |
| 顯示 | 800×480 RGB LCD |
| 觸控 | GT911 類 I²C 電容觸控 |
| 板類 | TouchMon／Waveshare 類 ESP32-S3 一體式顯示板 |
| 供電 | 穩定 5 V USB，建議最少 1 A |

### 1.1 點解有 16 MB flash，但設定寫 8 MB？

實機由燒錄工具偵測到 16 MB physical flash。不過目前部署使用經驗證的 `partitions_8MB_littlefs.csv`：

- 應用程式由 `0x10000` 開始；
- app partition 約 5.75 MB；
- LittleFS 由 `0x5D0000` 開始；
- 韌體另設 6 MB 產品容量上限。

呢個安排係為咗同已安裝裝置及資料位置保持一致。`partitions_16MB_littlefs.csv` 只供日後重新設計 OTA／partition 時使用。新手請勿自行切換，否則舊設定可能讀唔到，亦可能要完整 erase flash。

### 1.2 其他 ESP32 或其他屏幕

以下情況唔可以直接使用同一個 binary：

- 解像度唔係 800×480；
- SPI 或 MIPI 屏幕，而唔係目前 RGB wiring；
- LCD data pin、sync timing、backlight 或 touch pin 不同；
- 冇 8 MB PSRAM；
- 只得 4 MB flash；
- ESP32-C3、C6 或普通 ESP32，而唔係 ESP32-S3。

程式核心可以移植，但要修改 `include/esp_panel_board_custom_conf.h`、PlatformIO board、記憶體設定及畫面 layout。

## 2. 燒錄前準備

### 2.1 硬件

準備：

1. ESP32-S3 800×480 顯示板。
2. 一條確定支援 data transfer 嘅 USB-C 線。只可充電嘅線會令電腦完全睇唔到 ESP32。
3. 直接接駁電腦 USB 插口；首次燒錄避免用低質素 USB hub。
4. 找出板上 `BOOT` 及 `RESET` 按鈕。

### 2.2 軟件

需要：

- Git；
- Python 3；
- PlatformIO Core；
- Windows PowerShell 或 macOS Terminal。

PlatformIO 支援 Windows 及 macOS；官方亦建議一般使用者以普通帳戶安裝，唔需要使用 Administrator 或 `sudo`。如果你已經用 VS Code 的 PlatformIO IDE，可以用佢內置嘅 PlatformIO Core，毋須重複安裝。

官方參考：

- [PlatformIO 安裝總覽](https://docs.platformio.org/en/latest/core/installation/index.html)
- [PlatformIO 系統要求](https://docs.platformio.org/en/latest/core/installation/requirements.html)
- [GitHub Desktop clone 指引](https://docs.github.com/en/desktop/adding-and-cloning-repositories/cloning-and-forking-repositories/cloning-a-repository-from-github-desktop)

## 3. 下載專案

如果你係由零開始並選擇 Git 指令：Windows 請先完成第 5.1 節安裝 Git；macOS 請先完成第 6.2 節安裝 Command Line Tools。選擇 GitHub Desktop 就先安裝該應用程式，再用下面方法 B。

repo 地址：

```text
https://github.com/v2sdryan/ESP32_7inch_KMB_NOTE
```

### 方法 A：Git 指令

Windows PowerShell 或 macOS Terminal 都可以：

```bash
git clone https://github.com/v2sdryan/ESP32_7inch_KMB_NOTE.git
cd ESP32_7inch_KMB_NOTE
```

### 方法 B：GitHub Desktop

1. 安裝並開啟 GitHub Desktop。
2. 選擇 `File` → `Clone Repository`。
3. 切換到 `URL`。
4. 貼上 repo 地址。
5. 選擇本機資料夾，再按 `Clone`。
6. 用 Terminal／PowerShell 進入 clone 後嘅資料夾。

## 4. 私隱：唔好將設定上傳 GitHub

呢一步非常重要。

公開 repo 只應包含：

- `data/config.json.template`；
- source code；
- 不含個人資料的測試及文件。

以下內容永遠唔應該 commit：

- `data/config.json`；
- 家居、公司或學校 Wi-Fi SSID／密碼；
- Google Apps Script `DEVICE_TOKEN`；
- OAuth token、API key、client secret；
- 含個人日程、地址、IP、SSID或 token 嘅截圖；
- `.pio*` build cache、`.env`、log、firmware dump。

本 repo 嘅 `.gitignore` 已排除上述常見檔案，但使用者仍要喺每次 push 前檢查：

```bash
git status
git diff --cached
```

如果秘密曾經 push 上 GitHub，單純刪檔唔代表已安全，因為 git history 仍可能保留內容。請立即更換相關 Wi-Fi 密碼或 token，再清理 history。

## 5. Windows 安裝

以下步驟以 Windows 10／11 PowerShell 為例。

### 5.1 安裝 Git

方法 A（Git CLI）：

1. 由 [Git for Windows 官方網站](https://git-scm.com/download/win) 下載及安裝。
2. 關閉並重新開啟 PowerShell。
3. 確認：

```powershell
git --version
```

方法 B（唔想用 Git CLI）：安裝 [GitHub Desktop](https://desktop.github.com/)，依照第 3 章方法 B clone，再喺 clone 後資料夾開 PowerShell。GitHub Desktop 已處理 clone 所需元件，但本手冊後面嘅 build 指令仍然要喺 PowerShell 執行。

### 5.2 安裝 Python

1. 由 [python.org](https://www.python.org/downloads/windows/) 下載 Python 3。
2. 安裝畫面勾選 `Add Python to PATH`。
3. 開啟新 PowerShell，確認：

```powershell
py -3 --version
```

### 5.3 建立獨立環境

喺專案根目錄執行：

```powershell
py -3 -m venv .pio-venv
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.pio-venv\Scripts\Activate.ps1
python -m pip install --upgrade pip "platformio==6.1.19"
pio --version
```

`Set-ExecutionPolicy -Scope Process` 只影響目前 PowerShell 視窗，關閉後會還原。

之後每次重新開 PowerShell，只需：

```powershell
cd C:\你嘅路徑\ESP32_7inch_KMB_NOTE
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.pio-venv\Scripts\Activate.ps1
```

### 5.4 Windows USB／COM port

1. 用 data USB 線接駁 ESP32-S3。
2. 右鍵開始選單 → `Device Manager／裝置管理員`。
3. 展開 `Ports (COM & LPT)`。
4. 先拔 USB，記低消失咗邊個 COM；再插返 USB，重新出現嗰個就係 ESP32。
5. 記低例如 `COM7` 嘅 port。
6. 亦可以執行：

```powershell
pio device list
```

ESP32-S3 native USB 一般會顯示 USB Serial／JTAG；部分板經 CH343、CP210x 或其他 USB-UART 晶片連線，需要安裝板廠提供嘅 Windows driver。PlatformIO 官方亦提醒 Windows 使用者按板廠要求安裝 USB driver。見到 `Unknown device` 時，可開啟裝置 Properties → Details → Hardware Ids，將 VID/PID 同板廠規格核對；只由 Espressif、Waveshare、TouchMon 或 USB-UART 晶片廠官方頁下載 driver，唔好用不明 driver 網站。

## 6. macOS 安裝

### 6.1 檢查／安裝 Python 3

先檢查：

```bash
python3 --version
```

如顯示 command not found，請由 [Python 官方 macOS 下載頁](https://www.python.org/downloads/macos/) 安裝最新穩定 Python 3，完成後關閉並重新開啟 Terminal，再執行 `python3 --version`。如 Python installer 提供 `Install Certificates.command`，亦應按安裝說明完成，否則 PlatformIO 下載工具鏈時可能遇到 TLS certificate 問題。

### 6.2 安裝 Command Line Tools

開啟 Terminal：

```bash
xcode-select --install
```

如系統話已安裝，可以直接進行下一步。

### 6.3 建立獨立環境

喺專案根目錄執行：

```bash
python3 -m venv .pio-venv
source .pio-venv/bin/activate
python -m pip install --upgrade pip "platformio==6.1.19"
pio --version
```

之後每次重新開 Terminal：

```bash
cd ~/你嘅路徑/ESP32_7inch_KMB_NOTE
source .pio-venv/bin/activate
```

### 6.4 macOS serial port

接駁 ESP32 後執行：

```bash
pio device list
ls /dev/cu.usb* 2>/dev/null
```

常見名稱：

```text
/dev/cu.usbmodemXXXXXXXX
/dev/cu.usbserial-XXXXXXXX
```

燒錄時使用 `/dev/cu.*`，唔好使用 `/dev/tty.*`。

如果指令完全冇輸出，先拔走 ESP32，再插返 data USB 線並重跑；仍然冇輸出就直接跟第 9.3 節進入下載模式。

## 7. 最安全嘅首次 Wi-Fi 方法

建議唔好喺 source code 預載家居 Wi-Fi。保持 `data/config.json` 不存在，首次開機由 ESP32 救援 AP 設定。

裝置搵唔到已儲存 Wi-Fi 時會開啟：

```text
SSID: BusETA-Config
IP:   192.168.4.1
```

AP 密碼會喺每部 ESP32 第一次啟動時由硬件亂數產生，並獨立保存喺 NVS；佢唔係由 MAC address 推算，即使之後更新 LittleFS 亦會保持不變。畫面會以 `cfg-` 開頭顯示實際隨機密碼，例如：

```text
cfg-（每部裝置不同的隨機字串）
```

實際密碼會顯示喺裝置 Wi-Fi 資訊畫面；唔好將佢貼到 GitHub。

### 7.1 進階：本機預載 Wi-Fi

只有真係需要先做：

Windows：

```powershell
Copy-Item data\config.json.template data\config.json
notepad data\config.json
```

macOS：

```bash
cp data/config.json.template data/config.json
open -e data/config.json
```

填入本機資料後，`data/config.json` 會被 `.gitignore` 排除。仍然要用 `git status` 確認佢冇出現。

## 8. 編譯前測試

Windows／macOS 指令相同：

```bash
pio test -e native
```

所有測試應該顯示 `PASSED`。然後編譯 ESP32 韌體：

```bash
pio run -e esp32-s3-touch-lcd-7
```

成功時會見到：

```text
SUCCESS
Firmware size gate: ... / 5963776 bytes
```

如果韌體超過 6 MB，容量檢查會令 build 失敗；唔好移除呢個保護。

## 9. 第一次燒錄

第一次要寫入兩部分：

1. firmware；
2. LittleFS 網頁及資料。

### 9.1 Windows

將以下 `COM7` 改成你實際 port：

```powershell
pio run -e esp32-s3-touch-lcd-7 -t upload --upload-port COM7
pio device list
pio run -e esp32-s3-touch-lcd-7 -t uploadfs --upload-port COM7
```

### 9.2 macOS

將 port 改成你實際裝置：

```bash
pio run -e esp32-s3-touch-lcd-7 -t upload --upload-port /dev/cu.usbmodemXXXXXXXX
pio device list
pio run -e esp32-s3-touch-lcd-7 -t uploadfs --upload-port /dev/cu.usbmodemXXXXXXXX
```

第一條 upload 後，ESP32 可能重新枚舉，令 COM／`cu.usbmodem` 名稱改變。第二條 `uploadfs` 前要睇 `pio device list`；如 port 已變，使用新名稱。

### 9.3 Upload 連唔到：BOOT／RESET 下載模式

如果見到 `Failed to connect`、`No serial data received` 或完全冇 port：

1. 保持 USB 接駁。
2. 按住 `BOOT`（有啲板標作 `IO0`）。
3. 短按一下 `RESET`（亦可能標作 `RST` 或 `EN`），然後放開。
4. 等 2 秒。
5. 放開 `BOOT`。
6. 再執行 `pio device list`。
7. 用新出現嘅 COM／`cu.usbmodem` port 重新 upload。

部分板有「USB」及「UART」兩個 USB-C 插口。優先使用板廠標示支援 download／USB 嘅插口；唔肯定時查該硬件 revision 原理圖或說明書。亦可以拔 USB、按住 `BOOT`、重新插 USB、等 2 秒再放開 `BOOT`。進入下載模式時屏幕暫時熄滅係正常。Espressif 官方說明亦指出，未有自動下載電路嘅 ESP32-S3 可能需要手動令 GPIO0／BOOT 進入 download mode。

燒錄途中唔好拔 USB、按 RESET 或關閉 Terminal。

## 10. 首次開機及 Wi-Fi 設定

### 10.1 冇預載 Wi-Fi

1. 完成 upload 後按一次 `RESET`。
2. 等待屏幕啟動。
3. 喺手機／電腦 Wi-Fi 選擇 `BusETA-Config`。
4. 輸入屏幕顯示嘅 `cfg-` 隨機密碼。
5. 如果 captive portal 冇自動彈出，瀏覽：

```text
http://192.168.4.1
```

6. 開啟 `Wi-Fi 管理`。
7. 按 `掃描附近 Wi-Fi` 或 `手動新增`。
8. 輸入家居 Wi-Fi 密碼。
9. 按 `儲存 Wi-Fi 設定`。
10. 按 `重新啟動並連線`。

### 10.2 正常連線後

同一 Wi-Fi 下嘗試：

```text
http://buseta.local
```

如果 `.local` 喺某部手機、Windows browser 或 router 唔工作，使用屏幕顯示嘅實際 IP，例如：

```text
http://192.168.x.x
```

`.local` 使用 mDNS，唔係所有 guest Wi-Fi、企業 Wi-Fi、VPN、Private Relay、browser Secure DNS 或 AP isolation 都會容許。IP 網址正常而 `.local` 失敗，通常唔係韌體或網頁壞咗。

## 11. 設定巴士、待辦及天氣

### 11.1 巴士

1. 開啟首頁 → `巴士選站`。
2. 輸入路線，例如 `1A` 或 `93K`。
3. 選擇方向。
4. 等待完整車站清單載入。
5. 點選車站並加入收藏。
6. 最多四條收藏會顯示喺總覽。

### 11.2 本地待辦

1. 首頁 → `日程待辦`。
2. 輸入時間、標題及適用星期。
3. 儲存後總覽會更新。

Google Tasks bridge 係額外選項，純本地待辦唔需要 Google API、OAuth 或 Apps Script。

### 11.3 天氣

天氣使用香港天文台官方九日預報資料，畫面取首七日。裝置需要正常上網，資料唔會喺 AP-only 模式更新。

## 12. 安全更新韌體及處理設定

一般更新只 upload firmware，唔好執行 `uploadfs`；firmware upload 唔會改寫 `0x5D0000` 開始嘅 LittleFS，所以裝置內設定會保留。

macOS／Git Bash 完整流程：

```bash
OLD_COMMIT=$(git rev-parse HEAD)
git pull
source .pio-venv/bin/activate
pio test -e native
pio run -e esp32-s3-touch-lcd-7
pio run -e esp32-s3-touch-lcd-7 -t upload --upload-port 你的PORT
```

Windows PowerShell 完整流程：

```powershell
$OLD_COMMIT = git rev-parse HEAD
git pull
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.pio-venv\Scripts\Activate.ps1
pio test -e native
pio run -e esp32-s3-touch-lcd-7
pio run -e esp32-s3-touch-lcd-7 -t upload --upload-port COM7
```

### 12.1 點判斷需唔需要 uploadfs

完成 `git pull` 後，用上一步保存嘅舊 commit 檢查 LittleFS 來源有冇改。

macOS／Git Bash：

```bash
git diff --name-only "$OLD_COMMIT"..HEAD -- data scripts/stage_littlefs.py src/ui/ui_img_*.c
```

Windows PowerShell：

```powershell
git diff --name-only "${OLD_COMMIT}..HEAD" -- data scripts/stage_littlefs.py src/ui/ui_img_*.c
```

冇輸出：通常只需 firmware upload。

有輸出：網頁、資料或背景圖可能需要 `uploadfs`。

### 12.2 uploadfs 會清除裝置 runtime 設定

`uploadfs` 會重寫成個 LittleFS storage partition。呢個唔係「可能」：如果新 image 冇包含你目前裝置嘅 `config.json`，Wi-Fi、待辦、巴士收藏、Google Tasks token等 runtime 設定會消失。

最安全做法：

1. 平時只 upload firmware。
2. 真係需要 uploadfs 前，喺私人地方記錄所有設定；唔好貼 GitHub。
3. 如你一直維護本機、被 `.gitignore` 排除嘅 `data/config.json`，先更新佢至最新設定，再 build／uploadfs。
4. 如果你冇本機完整 config（一般 AP-first 使用者都冇），預期 uploadfs 後要重新連 `BusETA-Config`，再輸入 Wi-Fi、巴士收藏及待辦。
5. Google token 或其他秘密要由你自己嘅私人 secret store 重新填寫，唔好為方便而 commit。

執行 uploadfs 前最後確認：

```bash
git check-ignore -v data/config.json
```

如你建立咗 `data/config.json`，指令必須顯示佢由 `.gitignore` 排除。之後先執行：

```bash
pio run -e esp32-s3-touch-lcd-7 -t uploadfs --upload-port 你的PORT
```

## 13. 完成後檢查清單

- [ ] 屏幕正常顯示 800×480 畫面，冇偏移或閃爍。
- [ ] 觸控位置正確，四邊按鈕都按到。
- [ ] ESP32 能連上首選 Wi-Fi。
- [ ] `http://buseta.local` 或實際 IP 能開啟。
- [ ] `/wifi` 顯示已儲存 SSID，但唔顯示原有密碼。
- [ ] 巴士路線能載入方向及完整車站清單。
- [ ] 已選車站會顯示 ETA。
- [ ] 七日天氣有圖示、日期及溫度。
- [ ] 本地待辦儲存後會喺總覽出現。

## 14. 常見問題

### 電腦完全睇唔到 ESP32

- 換 data USB 線；
- 換電腦 USB 插口；
- 拔走 hub；
- 用 BOOT／RESET 進入下載模式；
- Windows 檢查 Device Manager 有冇未知裝置；
- 按板廠說明安裝 CH343／CP210x driver（如適用）。

### `port is busy`／`resource temporarily unavailable`

關閉 serial monitor、PlatformIO monitor、Arduino IDE 或其他正在使用同一個 port 嘅程式，等幾秒再試。唔好同時執行兩個 upload。

### 燒錄成功但屏幕全黑

- 按一次 RESET；
- 確認供電足夠；
- 確認硬件真係同一款 RGB panel／touch revision；
- 檢查 `include/esp_panel_board_custom_conf.h` pin mapping；
- 打開 serial monitor：

```bash
pio device monitor -b 115200 --port 你的PORT
```

### 觸控方向錯或按唔準

呢個通常代表 touch rotation、swap XY 或 mirror 設定同屏幕 revision 不同。唔好亂改多個 pin；先記錄實際按左上、右上、左下、右下時嘅座標，再調整 touch transform。

### IP 可以開，但 `buseta.local` 唔得

- 確認手機同 ESP32 喺同一個 LAN；
- 關閉 VPN 再試；
- 唔好使用 guest Wi-Fi；
- 如果係你自己管理嘅可信私人 LAN，而且明白裝置互相可見嘅風險，先考慮調整 AP／client isolation；guest／公共網絡應保持隔離並直接使用其他安全連線方式；
- Apple 裝置可用 Safari 測試；
- 直接用屏幕顯示嘅 IP；
- 想長期固定 IP，可喺 router 做 DHCP reservation，綁定 ESP32 MAC address。

### Wi-Fi 設錯後入唔返設定

裝置逐一嘗試已儲存 Wi-Fi，全部失敗後會自動開 `BusETA-Config`。最多可能要等數十秒。連線救援 AP 後開 `http://192.168.4.1/wifi` 修正。

### 天氣或 ETA 冇資料

- 先確認裝置有 internet，而唔係只連救援 AP；
- 開首頁狀態確認 Wi-Fi；
- 等候下一次自動更新；
- 檢查 router DNS、防火牆或 API 被封鎖；
- 巴士路線要先正確選擇方向及車站。

## 15. 公開分享前最後私隱檢查

推送到 GitHub 前執行：

```bash
git status --short
git diff --cached
```

再搜尋常見秘密字眼：

macOS：

```bash
git grep -n -i -E 'password|token|secret|api[_-]?key|/Users/'
```

Windows PowerShell：

```powershell
git grep -n -i -E "password|token|secret|api[_-]?key|C:\\Users\\"
```

搜尋結果可能只係安全程式碼、placeholder 或文件說明，仍然要逐項閱讀。任何真實密碼、SSID、token 或個人路徑都唔可以 push。

## 16. 取得協助時要提供咩

可以公開：

- ESP32 板完整型號；
- 屏幕解像度及 touch controller 型號；
- 作業系統版本；
- PlatformIO 錯誤最後 30–50 行；
- 韌體版本／commit hash；
- 已遮走 SSID、IP、MAC、token及個人日程嘅畫面。

唔好公開：Wi-Fi 密碼、AP 密碼、完整 `data/config.json`、Google token、OAuth secret、未遮資料嘅 serial log或截圖。
