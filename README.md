# AeroPKG (Title ID: APKG00001)

A clean, fast, hardware-accelerated PlayStation 4 homebrew package manager, dashboard customizer, and remote management suite for jailbroken PS4 consoles running GoldHEN (Firmware up to 13.52+).

---

## Key Features

1. **Clean, Fast, & 60 FPS Native UI**:
   - Built with C++17, OpenOrbis, and hardware-accelerated SDL2/OrbisGL.
   - Snappy navigation with DualShock 4 controls, LRU texture caching, and instant startup with zero lag.

2. **Dashboard & App Customizer**:
   - **Rename Apps**: Edit the displayed title of any installed game or homebrew directly.
   - **Icon & Art Customizer**: Swap `icon0.png` and background art from USB or wirelessly from your PC.
   - **Interactive XMB Reordering**: Visually move games up or down with the D-Pad to determine exact launch order (1st, 2nd, 3rd, last on the PS4 Home Screen) or apply 1-click A-Z / Size presets.
   - **Safe Database Guard**: Automated SQLite backups (`app.db.aeropkg_bak`) before any mutation with one-click restore.
   - **Soft ShellCore Reload**: Instantly refreshes the PS4 XMB home screen without a full console reboot.

3. **PKG Installation Engine**:
   - **USB Batch Installer**: Scan `/mnt/usb0` or `/mnt/usb1` for `.pkg` files and install them individually or via **"Install All"** with an asynchronous queue tracker.
   - **Official BGFT Integration**: Installs queue directly in the official PS4 Notifications and Download Manager, enabling background downloads and pause/resume.

4. **Embedded Web UI & Remote Package Sender**:
   - Access `http://<PS4_IP>:8080` from any web browser on your PC or smartphone.
   - Send package download links directly to your PS4 (fully compatible with Remote Package Sender v2 clients).
   - Drag-and-drop custom icons and crop custom avatars from your browser.

5. **FPKGi `games.json` Catalog**:
   - 100% compatible with standard FPKGi `games.json` format.
   - Supports local files (`/data/AeroPKG/games.json` or USB) and remote repository URLs with fast in-memory search and filtering.

6. **User Avatar Suite**:
   - Change your PS4 user profile avatar directly from USB or web upload.
   - Includes offline presets and automatic scaling to 440x440.

7. **Storage & Download Cleaner**:
   - Scans `/data/pkg/`, `/user/download/`, and orphaned cache chunks to safely reclaim gigabytes of storage with one click.

---

## Controller Controls (DualShock 4)

| Button | Action |
|---|---|
| **L1 / R1** | Switch between tabs (Dashboard, USB, FPKGi, Avatars, Cleaner) |
| **D-Pad Up / Down** | Navigate items |
| **Cross (X)** | Select / Confirm |
| **Circle (O)** | Back / Cancel |
| **Square** | Toggle XMB Slot Move Mode (in Dashboard) / Batch Install (in USB tab) |
| **Triangle** | Sort Presets (A-Z) / Scan USB or Cache |
| **Options (Start)** | Apply XMB Order & Soft-Restart ShellCore |

---

## Building the Package (`AeroPKG.pkg`)

### Option 1: Automatic Cloud Build (Recommended)
This repository includes a pre-configured GitHub Actions CI workflow in `.github/workflows/build.yml`.
Simply push to GitHub or trigger the workflow under the **Actions** tab to download the compiled `AeroPKG.pkg` directly from GitHub Artifacts.

### Option 2: Local OpenOrbis Toolchain Build
If you have OpenOrbis Toolchain installed:
```bash
# Clone the repository
git clone https://github.com/your-username/AeroPKG.git
cd AeroPKG

# Build ELF and package
make pkg
```
This produces `AeroPKG.pkg`, which can be installed via GoldHEN Package Installer.
