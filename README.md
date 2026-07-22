# ESP32_7inch_KMB_NOTE

香港家庭資訊屏韌體，為 ESP32-S3、800×480 RGB LCD 及觸控屏幕而設。預設總覽同時顯示今日待辦、最多四條巴士 ETA，以及香港天文台七日天氣。

## 已驗證硬件

- ESP32-S3
- 8 MB OPI PSRAM
- 實際 16 MB internal flash
- 800×480 RGB LCD
- GT911 類 I²C 電容觸控
- TouchMon／Waveshare 類一體式顯示板

目前 `platformio.ini` 刻意沿用經實機驗證的 8 MB-compatible factory partition layout；雖然晶片實際有 16 MB flash，請勿只因容量較大便自行更改 partition table。詳情見安裝手冊。

## 功能

- 九巴 ETA、路線搜尋、方向及車站選擇
- 香港天文台七日天氣、彩色天氣圖示
- 本地今日／每週待辦
- 最多四個 Wi-Fi、首選次序及救援 AP
- 手機／電腦網頁管理：`http://buseta.local`
- 找不到 `.local` 時顯示實際 IP 作後備
- 800×480 觸控總覽及原版詳細頁
- Native tests 與 6 MB 韌體容量閘門

## 新手安裝

請依照 [Windows／macOS 詳細安裝及燒錄手冊](docs/INSTALL_ZH_HK.md)。內容包括：

- 軟件及 USB 準備
- Windows PowerShell 完整指令
- macOS Terminal 完整指令
- BOOT／RESET 下載模式
- 首次 Wi-Fi 設定
- `buseta.local` 與 IP fallback
- 安全更新韌體，以及需要更新 LittleFS 時嘅設定處理
- 常見錯誤排查

## 私隱與安全

公開 repo **不包含**任何家居 Wi-Fi SSID、Wi-Fi 密碼、Google token、API secret、個人日程或實機設定。

- `data/config.json` 已被 `.gitignore` 排除。
- 只提交不含秘密的 `data/config.json.template`。
- 現有 Wi-Fi 密碼不會由 ESP32 傳回管理網頁。
- 請勿把實機 config、serial log、含私人資料的截圖或 token 貼到 GitHub Issue。

## 開發指令

```bash
python3 -m venv .pio-venv
source .pio-venv/bin/activate
python -m pip install --upgrade pip "platformio==6.1.19"
pio test -e native
pio run -e esp32-s3-touch-lcd-7
```

Windows PowerShell 請使用：

```powershell
py -3 -m venv .pio-venv
.\.pio-venv\Scripts\Activate.ps1
python -m pip install --upgrade pip "platformio==6.1.19"
pio test -e native
pio run -e esp32-s3-touch-lcd-7
```

## 主要資料夾

```text
include/             顯示板與 LVGL 設定
src/                 ESP32 韌體及內嵌管理頁
data/                LittleFS 網頁及公開資料
test/                Native 單元測試
scripts/             LittleFS staging、容量檢查及資料更新工具
docs/                使用、設計及安裝文件
```

## 授權

本專案由作者原創並有權授權的程式碼及文件採用 [MIT License](LICENSE)。你可以用於個人、教育及商業用途，亦可以使用、複製、修改及分發，但必須保留原有版權及授權聲明。

第三方程式庫、字型、地圖資料、圖片及其他資源不會因本專案採用 MIT License 而改變授權；它們仍沿用各自原有的授權及聲明。
