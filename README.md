# CorsairTray

Ícone na bandeja do Windows com o nível de bateria do headset Corsair conectado, lido via iCUE SDK.

| Bateria   | Cor      |
|-----------|----------|
| 100%      | Verde    |
| 50–99%    | Azul     |
| 20–49%    | Amarelo  |
| < 20%     | Vermelho |
| sem dados | Cinza (`?`) |

O nível é atualizado a cada 30 s e quando o iCUE conecta. Clique direito no ícone: **Atualizar** / **Sair**. Duplo clique também atualiza.

## Requisitos

- Windows 10/11 x64
- iCUE instalado e aberto, com **controle de software de terceiros** habilitado nas configurações
- Visual Studio 2019+ e CMake 3.20+
- [NSIS](https://nsis.sourceforge.io/), apenas para gerar o instalador
- [iCUE SDK](https://github.com/CorsairOfficial/cue-sdk/releases) (testado com 4.0.84)

## Build

Os headers do SDK já estão em `third_party/`. A DLL e a `.lib` não são versionadas — copie do SDK baixado:

```
iCUESDK/lib/x64/iCUESDK.x64_2019.lib      -> third_party/
iCUESDK/redist/x64/iCUESDK.x64_2019.dll   -> third_party/
```

Depois:

```bash
cmake -S . -B build
cmake --build build --config Release
```

O executável fica em `build/src/Release/CorsairTray.exe`, com a DLL copiada ao lado automaticamente.

## Gerar o instalador

Requer o [NSIS](https://nsis.sourceforge.io/) instalado. Depois do build em Release:

```bash
cd build
cpack -C Release
```

Gera dois pacotes em `build/`:

- `CorsairTray-<versão>-win64.exe` — instalador (pede admin; instala em `Program Files\CorsairTray`, cria atalho no menu Iniciar, inicia com o Windows e tem desinstalador)
- `CorsairTray-<versão>-win64.zip` — versão portátil

Os dois já incluem a DLL do iCUE SDK e o runtime do Visual C++, então não é preciso instalar nada além do iCUE na máquina de destino. A versão é definida em `project(... VERSION ...)` no `CMakeLists.txt`.

> O instalador não é assinado digitalmente, então o Windows SmartScreen pode mostrar "Editor desconhecido" na primeira execução.
