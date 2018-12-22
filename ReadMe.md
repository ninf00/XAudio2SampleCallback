XAudio2SampleCallback
====================================================================================================

これは何？
----------------------------------------------------------------------------------------------------

DirectX9上で動作するXAudio2でwavファイルとoggファイルを再生のためのサンプルです。  
勉強用に作ったのでとりあえず音を出すことしか考えていないです。


事前準備
----------------------------------------------------------------------------------------------------

DirectX SDKをお使いのコンピューターにインストールしてください。

- [DirectX SDK](https://www.microsoft.com/en-us/download/details.aspx?id=6812)

Ogg Vorbisライブラリをインストールしてください。

- [Ogg Vorbis](https://www.xiph.org/downloads/)

インストール方法とライブラリのビルド方法など  
http://marupeke296.com/OGG_No1_LibraryInstall.html

上記手順を行うと最終的に
- libogg_static.lib
- libvorbis_static.lib
- libvorbisfile_static.lib
の3つのライブラリファイルが出来上がります


インストールと使い方
----------------------------------------------------------------------------------------------------

gitコマンドを使ってcloneして生成されたフォルダの中の
ソリューションファイルをVisual Studio 2017で開いて下さい。  
続いて事前準備で用意したOgg Vorbisライブラリへのパスを通します。  
基本的な通し方は以下のリンク先を参考にして下さい。  
http://marupeke296.com/OGG_No1_LibraryInstall.html

簡単に手順を説明すると以下の通りです
1. ライブラリのヘッダーファイルへのパスを通す  
プロジェクトのプロパティを開く  
Configuration Properties->C/C++->General  
Additional Include DirectoriesにインストールしたOgg Vorbisフォルダ直下にあるincludeディレクトへのパスを追加

2. ライブラリがあるフォルダへのパスを通す  
プロジェクトのプロパティを開く  
Configuration Properties->Linker->General  
Additional Library DirectoriesにインストールしたOgg Vorbisのライブラリファイルがあるフォルダへのパスを追加  
※事前準備で作成した3つのライブラリファイルがあるフォルダ
3. 使用するライブラリをリンクする  
プロジェクトのプロパティを開く  
Configuration Properties->Linker->Input  
Additional Dependenciesに事前準備で用意した3つのライブラリファイルを追加  
- libogg_static.lib
- libvorbis_static.lib
- libvorbisfile_static.lib


まずはビルドしてください。  
XAudio2SampleCallback.cppにあるPLAY_MODEのマクロの値で再生方法切り替わります。  

0: 直接MMIOを操作してwaveファイルを再生  
1: WaveOperatorクラスを介してwaveファイルを操作して再生  
2: WaveOperatorクラスを利用、BGM想定の音楽はストリームスレッドを生成して再生  
-> 同時にSE想定の音楽はメモリに読み込んで再生  
3: OggOperatorクラスを利用、挙動は2と同じだが再生するファイルがoggファイルになる

exeが生成されたらexeと同じフォルダに再生させたいwavファイルを  
test.wavという名前で設置してください。  
同様にoggファイルもtest.oggという名前で設置してください。  
上記のパターン2を使う場合はSEとして再生したいwavファイルを  
exeと同じフォルダにse01.wavという名前で設置してください。  
上記のパターン3を使う場合はSEとして再生したいoggファイルをexeと同じフォルダにse01.oggという名前で設置してください。    

その後exeを実行すると黒いウインドウが表示されるので
何かキーを押すと再生が始まるはずです。  

再生が終わるとウインドウに"play end!"と表示されます。  
そうなったら、何かキーを入力するとアプリケーションは終了します。


動作環境
----------------------------------------------------------------------------------------------------

 - Windows 10 Pro 64bit
 - Visual Studio 2017 Community Edition 

※ これ以外の環境でも動作するかもしれませんが動作確認はしていません



主な機能
----------------------------------------------------------------------------------------------------

### wavファイル再生
exeと同じフォルダに置かれたtest.wavを再生します。
パターン2の場合はse01.wavも再生します。

### wavファイルの情報表示
再生中のwavファイルの基本情報をコマンドプロンプトのウインドウに出力します。

### oggファイル再生
exeと同じフォルダに置かれたtest.oggを再生します。

### oggファイルの情報表示
再生中のoggファイルの基本情報をコマンドプロンプトのウインドウに出力します。

### Reference

以下のサイトのコードを参考にさせて頂きました。

- [Xaudio2を使う(@tobira-code)](https://qiita.com/tobira-code/items/39936c4e2b1168fb79ce#sample-code%E5%85%A8%E4%BD%93)

- [Ogg Vorbis入門編(@Marupeke_IKD)](http://marupeke296.com/OGG_main.html)


### License

[MIT License](LICENSE)