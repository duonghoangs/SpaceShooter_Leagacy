# SpaceShooter Legacy

SpaceShooter Legacy là phiên bản cải tiến từ game bắn thiên thạch C++/SDL2 gốc do **Hoàng Thái Dương** tự xây dựng. Dự án giữ lại tinh thần, lối chơi và tài nguyên pixel-art của bản đầu tiên, đồng thời tái cấu trúc toàn bộ mã nguồn để game mượt hơn, ổn định hơn và dễ tiếp tục phát triển.

Phiên bản mới bổ sung fixed timestep, nội suy chuyển động, collision chính xác, quản lý tài nguyên bằng RAII, kiến trúc state-based, UI hoàn chỉnh và hệ thống hiệu ứng hình ảnh theo thời gian thực. Các ảnh nền cũ được nâng cấp nhưng vẫn giữ bố cục và phong cách retro nguyên bản.

## Hình ảnh

| Menu | Gameplay |
| --- | --- |
| ![Menu](docs/screenshots/menu.png) | ![Gameplay](docs/screenshots/gameplay.png) |

## Điểm cải tiến nổi bật

- Gameplay chạy ở fixed timestep 60 Hz, render tối đa 120 Hz và nội suy vị trí giữa các simulation tick.
- Chuyển động tàu, đạn và thiên thạch mượt, không phụ thuộc FPS.
- Collision của đạn dùng swept segment, hạn chế xuyên thiên thạch khi vận tốc cao.
- Tàu, đạn, thiên thạch và particle dùng pool cố định; hot path không cấp phát heap.
- Hiệu ứng động gồm lửa động cơ, muzzle flash, glow, bóng đổ, rung màn hình, hit flash, particle burst và shockwave.
- Hệ thống wave tự tăng số lượng và tốc độ thiên thạch.
- Menu, HUD, pause overlay và game-over flow hỗ trợ bàn phím lẫn chuột.
- Âm thanh là tùy chọn; game vẫn build và chạy khi không có SDL2_mixer.
- Có test logic, benchmark, CMake, Makefile và GitHub Actions CI.

## Kiến trúc

```text
.
├── assets/
│   ├── audio/                  # Nhạc và hiệu ứng âm thanh tùy chọn
│   └── images/                 # Texture gốc và texture đã nâng cấp
├── include/game/
│   ├── core/                   # Application, graphics, audio, assets
│   ├── entities/               # Player, Bullet và AsteroidField
│   ├── math/                   # Vector2
│   ├── states/                 # Menu, Playing và GameOver
│   └── ui/                     # Bitmap font, panel và button renderer
├── src/                        # Phần triển khai
├── tests/                      # Test logic và benchmark
├── CMakeLists.txt
└── Makefile
```

`Application` sở hữu SDL runtime, tài nguyên và state hiện tại. Mỗi state tách riêng xử lý input, update và render. Texture được tải một lần khi khởi động và có một owner duy nhất.

## Yêu cầu

- Trình biên dịch hỗ trợ C++20
- CMake 3.20 trở lên hoặc Make
- SDL2
- SDL2_image
- SDL2_mixer, tùy chọn

## Build bằng CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/asteroids
```

Tắt audio khi cần:

```bash
cmake -S . -B build -DASTEROIDS_ENABLE_AUDIO=OFF
```

## Build nhanh bằng Make

```bash
make
make test
make benchmark
make run
```

Makefile hỗ trợ SDL framework trên macOS và `pkg-config` trên Linux.

## Điều khiển

- `↑`: tăng tốc
- `←` / `→`: xoay tàu
- `Space`: bắn hoặc xác nhận menu
- `Enter`: xác nhận menu
- `P` hoặc `Esc`: tạm dừng khi đang chơi

## Âm thanh tùy chọn

Đặt các file sau trong `assets/audio/` để bật đầy đủ âm thanh:

- `background.mp3`
- `death.mp3`
- `fire.wav`
- `asteroid.wav`

## Chụp UI

Game có chế độ chụp toàn bộ màn hình chính để review giao diện:

```bash
./build/asteroids --capture-ui output/ui
```

## Tác giả

**Hoàng Thái Dương**

Mã sinh viên: **23021508**

Dự án này là quá trình hiện đại hóa và phát triển tiếp từ game Space Shooter/Asteroids nguyên bản của tác giả.
