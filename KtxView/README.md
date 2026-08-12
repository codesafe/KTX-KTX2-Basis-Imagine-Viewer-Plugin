# KtxView — Imagine View용 KTX / KTX2 / Basis Universal 로더 플러그인

Imagine Viewer에서 웹용 텍스처 포맷을 볼 수 있게 하는 플러그인입니다.
- [Imagine Viewer](https://www.nyam.pe.kr/dev/imagine/) — Imagine Viewer는 여기에서 다운로드


## 지원 포맷

| 확장자 | 컨테이너 | 지원 내용 |
|---|---|---|
| `.ktx` | KTX1 | 비압축(RGBA8/RGB8/BGRA/BGR/R8/RG8/L8/LA8), ETC1, ETC2(RGB/A1/RGBA), EAC(R11/RG11), BC1~BC7(DXT1/3/5, RGTC, BPTC), BC6H(HDR→클램프), **ASTC LDR 4x4~12x12(UNORM/sRGB)** |
| `.ktx2` | KTX2 | Basis ETC1S(BasisLZ) / UASTC LDR, raw VkFormat(비압축/BCn/ETC2/EAC), **raw ASTC LDR 및 ASTC HDR(SFLOAT) 전 블록 크기**, zstd supercompression |
| `.basis` | Basis Universal | ETC1S / UASTC (HDR 포함, 클램프 표시) |

- 표시: 항상 32bpp BGRA. 알파 보유 포맷은 알파 블렌딩 활성화.
- 밉맵/큐브맵/배열/3D: base level, 첫 face/layer/slice만 표시.
- ASTC는 블록 크기 14종(4x4, 5x4, 5x5, 6x5, 6x6, 8x5, 8x6, 8x8, 10x5, 10x6, 10x8, 10x10, 12x10, 12x12)을 모두 지원하며,
  HDR CEM을 쓰는 블록은 half로 디코딩 후 클램프한다. sRGB 변종은 sRGB 디코드 프로파일을 사용.
- 미지원: SNORM(EAC/RGTC signed) — "지원하지 않는 형식" 오류로 안전 처리.
- 저장(인코딩)은 지원하지 않음.

## 빌드 (Visual Studio 2022)

```
MSBuild KtxView.sln /p:Configuration=Release /p:Platform=x64
```

- 출력: `bin\Release\KtxView.plg64` (x64, 정적 CRT `/MT`)
- SDK 헤더 `ImagPlug.H`는 `..\__\` 경로에서 include (SDK 폴더 구조 그대로 사용)

## 설치

`KtxView.plg64`를 Imagine 실행 파일(`Imagine64.exe`) 옆의 `Plugin\` 폴더에 복사하면 끝.
Imagine 메뉴의 'Installed plugins'에서 등록 확인 가능.

## 구조

```
src\Main.cpp        플러그인 진입점(ImaginePluginGetInfoA/W), 파일 타입 3종 등록
src\Common.cpp      RGBA8 중간버퍼 → Imagine 비트맵(BGRA) 공통 루틴, 가변 블록 크기 디코더 디스패치
src\KtxLoader.cpp   KTX1 파서 (glInternalFormat → 공통 포맷 매핑)
src\Ktx2Loader.cpp  KTX2: vkFormat==0만 basis 트랜스코더로, 나머지는 raw 경로(+zstd)
src\BasisLoader.cpp .basis 로더
third_party\        basis_universal transcoder(Apache-2.0), zstd 디코더, bcdec/etcdec(MIT/Unlicense)
```

## 서드파티

- [basis_universal](https://github.com/BinomialLLC/basis_universal) — transcoder + zstd 디코더 + ASTC 디코더(`basisu_astc_helpers.h`) (Apache-2.0)
- [bcdec](https://github.com/iOrange/bcdec) / [etcdec](https://github.com/iOrange/etcdec) — BCn/ETC 소프트웨어 디코더 (MIT/Unlicense)
