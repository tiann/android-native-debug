# 如何调试Android Native Framework

半年前写了一篇文章，介绍 [如何调试Android Framework][1]，但是只提到了Framework中Java代码的调试办法，但实际上有很多代码都是用C++实现的；无奈当时并并没有趁手的native调试工具，无法做到像Java调试那样简单直观（gdb+eclipse/ida之流虽然可以但是不完美），于是就搁置下了。

Android Studio 2.2版本带来了全新的对Android Native代码的开发以及调试支持，另外LLDB的Android调试插件也日渐成熟，我终于可以把这篇文章继续下去了！本文将带来Android Framework中native代码的调试方法。

在正式介绍如何调试之前，必须先说明一些基本的概念。调试器在调试一个可执行文件的时候，必须知道一些调试信息才能进行调试，这个调试信息可多可少（也可以没有）。最直观的比如行号信息，如果调试器知道行号信息，那么在进行调试的时候就能知道当前执行到了源代码的哪一行，如果调试器还知道对应代码的源文件在哪，那么现代IDE的调试器一般就能顺着源码带你飞了，这就是所谓的源码调试。相反，如果没有行号和源码信息，那么只能进行更低级别的调试了，调试器只能告诉你一些寄存器的值；而当前运行的代码也只是PC寄存器所指向的二进制数据，这些数据要么是虚拟机指令，要么是汇编指令；这就是所谓的无源码调试。显然无源码调试相比源码级别的调试要麻烦的多；接下来将围绕这两个方面分别介绍。

## 用Android Studio进行源码调试

如上文所述，如果需要实现源码调试，必须知道足够的调试信息；在native调试中就是所谓的「调试符号」。但是release版本的动态链接库或者可执行文件一般并不会包含我们需要的调试信息，在Android系统中，`/system/lib/*` 目录下的那些系统so并没有足够的调试信息，因此如果要进行源码调试，必须自己编译Android源代码，才能获取调试信息，进而让调试器协助我们调试。

Android源码编译是个麻烦事儿，我写过一篇文章介绍 [如何使用Docker调试](https://zhuanlan.zhihu.com/p/24633328?refer=weishu) ；但是，Android版本众多，如果真的需要调试各个版本，在本地进行编译几乎是不可能的——一个版本约占60G空间，如果每个版本都编译，你的Mac还有空间可用吗？因此比较推荐使用云服务进行源码编译；比如使用阿里云的ECS，20M的网速15分钟就能下载完源码；编译速度还勉强，4核8G两个半小时。扯远了 :) 如果你没有精力编译Android源码，我这个 [Demo工程][2] 可以让你尝尝鲜，里面包含一些调试的必要文件，可以体会一下Native调试的感觉。

如果我们已经拥有了调试符号，那么还需要保证你的符号文件和设备上真正运行的动态链接库或者可执行文件是对应的，不然就是鸡同鸭讲了。最简单的办法就是使用模拟器。我们编译完源码之后，一个主要的编译产物就是 `system.img`，这个 `system.img`会在启动之后挂载到设备的 /system 分区，而system分区包含了Android系统运行时的绝大部分可执行文件和动态链接库，而这些文件就是我们的编译输出，正好可以与编译得到的调试符号进行配合调试。模拟器有一个 `-system`选项用来指定模拟器使用的 system.img文件；于是这个问题也解决了。

最后一个问题就是，既然是源码调试，当然需要源码了；我们可以在 [AOSP](https://android.googlesource.com/) 上下载需要的源码即可；需要注意的是，在check分支的时候，必须保证你的分支和编译源码时候的分支是一致的。

需要说明的是，虽然我们使用Android Studio调试，但是其背后的支撑技术实际上是 [LLDB调试器](http://lldb.llvm.org/)。LLDB是一个相当强大的调试器，如果你现在还不知道它为何物，那真的是孤陋寡闻了！建议先简单学习一下 [教程](http://lldb.llvm.org/index.html)

万事俱备，Let's go!

### 建立Android Studio工程

实际上任何Android Studio工程都可以进行native源码调试，但是为了方便还是新建一个工程；这个工程是一个空工程，没有任何实际用途；为了体验方便，你可以使用我的这个 [Demo][2] 工程，里面包含了调试符号以及模拟器需要使用的system.img。一定要注意Android Studio的版本必须是2.2以上（我的是2.2.3稳定版)。

### 下载需要调试模块的源码

如果你本地编译了Android源码，那么就不需要这一步了；但是更多的时候我们只是想调试某一个模块，那么只需要下载这个模块的源码就够了。我这里演示的是调试 ART 运行时，因此直接下载ART模块的源码即可，我编译的Android源码版本是 `android-5.1.1_r9`，因此需要check这个分支的源码，地址在这里：[ART-android-5.1.1_r9](https://android.googlesource.com/platform/art/+/android-5.1.1_r9)

### 运行模拟器

由于我们的调试符号需要与运行时的动态链接库对应，因此我们需要借助模拟器；首先创建一个编译出来的调试符号对应的API版本的模拟器，我这里提供的是5.1.1也就是API 22；然后使用编译出来的 system.img 启动模拟器（[Demo]工程的image目录有我编译出来的文件，可以直接使用。）：

```shell
emulator -avd 22 -verbose -no-boot-anim -system /path/to/system.img
```

这个过程灰常灰常长！！我启动这个模拟器花了半个多小时，也是醉。现在是2017年，已经是Android创建的第十个年头，ARM模拟器还是烂的一塌糊涂，无力吐槽。一个能让它快一点的诀窍是创建一个小一点的SD card；我设置的是10M。

### 开始调试

#### 选择native调试模式

首先我们对调试的宿主工程设置一下，选择native调试功能。点击运行下面的按钮 `Edit Configuration`：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484287940736.png" width="232"/>

然后在debugger栏选择Native：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484288018710.png" width="400"/>

然后我们点击旁边的 `Debug`小按钮运行调试程序：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484288078862.png" width="183"/>

#### 设置调试符号以及关联源码

在运行程序之后，我们可以在Android Studio的状态栏看到，LLDB调试插件自动帮我们完成了so查找路径的过程，这一点比gdb方便多了！在Android Studio的Debug窗口会自动弹出来，如下：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484288380978.png" width="474"/>

我们点击那个 `pause program` 按钮，可以让程序暂停运行：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484288440156.png" width="615"/>

上图左边是正在运行的线程的堆栈信息，右边有两个tab，一个用来显示变量的值；一个是lldb交互式调试窗口！我们先切换到lldb窗口，输入如下命令设置一个断点：

> (lldb) br s -n CollectGarbageInternal
Breakpoint 2: where = libart.so`art::gc::Heap::CollectGarbageInternal(art::gc::collector::GcType, art::gc::GcCause, bool), address = 0xb4648c20

可以看到，断点已经成功设置；这个断点在libart.so中，不过现在还没有调试符号信息以及源码信息，我们只知道它的地址。接下来我们设置调试符号以及关联源码。

接下来我们把编译得到的符号文件 libart.so 告诉调试器（符号文件和真正的动态链接库这两个文件名字相同，只不过一个在编译输出的symbols目录) ；在lldb窗口执行：

```shell
(lldb) add-dsym /Users/weishu/dev/github/Android-native-debug/app/symbols/libart.so
symbol file '/Users/weishu/dev/github/Android-native-debug/app/symbols/libart.so' has been added to '/Users/weishu/.lldb/module_cache/remote-android/.cache/C51E51E5-0000-0000-0000-000000000000/libart.so'
```

注意后面那个目录你的机器上与我的可能不同，需要修改一下。我们再看看有什么变化，看一下刚刚的断点：

> (lldb) br list 2
2: name = 'CollectGarbageInternal', locations = 1, resolved = 1, hit count = 0
  2.1: where = libart.so`art::gc::Heap::CollectGarbageInternal(art::gc::collector::GcType, art::gc::GcCause, bool) **at heap.cc:2124**, address = 0xb4648c20, resolved, hit count = 0 

行号信息已经加载出来了！！在 `heap.cc` 这个文件的第2124行。不过如果这时候断点命中，依然无法关联到源码。我们看一下调试器所所知道的源码信息：

> (lldb) source info
Lines found in module `libart.so
[0xb4648c20-0xb4648c28): **/Volumes/Android/android-5.1.1_r9/art/runtime/gc/heap.cc**:2124

纳尼？？这个目录是个什么鬼，根本没有这个目录好伐？难道是调试器搞错了？

在继续介绍之前我们需要了解一些关于「调试符号」的知识；我们拿到的调试符号文件其实是一个DWARF文件，只不过这个文件被嵌入到了ELF文件格式之中，而其中的调试符号则在一些名为 `.debug_*` 的段之中，我们可以用 `readelf -S libart.so` 查看一下：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484289374465.png" width="616"/>

编译器在编译libart.so的时候，记录下了**编译时候**源代码与代码偏移之间的对应关系，因此调试器可以从调试符号文件中获取到源码行号信息；如下：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484289826696.png" width="486"/>

这下我们明白了上面那个莫名其妙的目录是什么了；原来是在编译`libart.so`的那个机器上存在源码。那么问题来了，我们绝大多数情况下是使用另外一台机器上的源码进行调试的——比如我提供的那个 [Demo工程][2] 包含的带符号libart.so里面保存的源文件信息的目录实际上是我编译的电脑上的目录，而你调试的时候需要使用自己电脑上的目录。知道了问题所在，解决就很简单了，我们需要映射一下；在Android Studio的Debug 窗口的lldb 那个tab执行如下命令：

```shell
(lldb) settings set target.source-map /Volumes/Android/android-5.1.1_r9/ /Users/weishu/dev/github/Android-native-debug/app/source/
```

第一个参数的意思是编译时候的目录信息，第二个参数是你机器上的源码存放路径；设置成自己的即可。

这时候，我们再触发断点（点击demo项目的Debug按钮），看看发生了什么？！

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484290490320.png" width="1191"/>

至此，我们已经成功滴完成了在Android Studio中Native代码的源码调试。你可以像调试Java代码一样调试Native代码，step/in/out/over，条件断点，watch point任你飞。你可以借助这个工具去探究Android底层运行原理，比如垃圾回收机制，对象分配机制，Binder通信等等，完全不在话下！

## 无源码调试

接下来再介绍一下操作简单但是使用门槛高的「无源码调试」方式；本来打算继续使用Android Studio的，但是无奈现阶段还有BUG，给官方提了issue但是响应很慢：https://code.google.com/p/android/issues/detail?id=231116。因此我们直接使用 LLDB 调试；当然，用gdb也能进行无源码调试，但是使用lldb比gdb的步骤要简单得多；不信你可以看下文。

### 安装Android LLDB工具

要使用lldb进行调试，首先需要在调试设备上运行一个lldb-server，这个lldb-server attach到我们需要调试的进程，然后我们的开发机与这个server进行通信，就可以进行调试了。熟悉gdb调试的同学应该很清楚这一点。我们可以用Android Studio直接下载这个工具，打开SDK Manager：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484280931189.png" width="720"/>

如上图，勾选这个即可；下载的内容会存放到你的 $ANDROID_SDK/lldb 目录下。

### 使用步骤

安装好必要的工具之后，就可以开始调试了；整体步骤比较简单：把lldb-server推送到调试设备并运行这个server，在开发机上连上这个server即可；以下是详细步骤。

#### 在手机端运行lldb-server

如果你的调试设备是root的，那么相对来说比较简单；毕竟我们的调试进程lldb-server要attach到被调试的进程是需要一定权限的，如果是root权限那么没有限制；如果没有root，那么我们只能借助`run-as`命令来调试自己的进程；另外，被调试的进程必须是debuggable，不赘述。以下以root的设备为例（比如模拟器）

1. 首先把lldb-server push到调试设备。lldb-sever这个文件可以在 `$ANDROID_SDK/lldb/<版本号数字>/android/ 目录下找到，确认你被调试设备的CPU构架之后选择你需要的那个文件，比如大多数是arm构架，那么执行：

    ```shell
    adb push lldb-server /data/local/tmp/
    ```

2. 在调试设备上运行lldb-server。
    
    ```shell
    adb shell /data/local/tmp/lldb-server platform \
    --server --listen unix-abstract:///data/local/tmp/debug.sock
    ```
    
    如果提示 /data/local/tmp/lldb-server: can't execute: Permission denied，那么给这个文件加上可执行权限之后再执行上述命令：
    
    ```shell
    adb shell chmod 777 /data/local/tmp/lldb-server
    ```
    
    这样，调试server就在设备上运行起来了，注意要这么做需要设备拥有root权限，不然后面无法attach进程进行调试；没有root权限另有办法。另外，这个命令执行之后所在终端会进入阻塞状态，不要管它，如下进行的所有操作需要重新打开一个新的终端。
    
#### 连接到lldb-server开始调试

首先打开终端执行lldb（Mac开发者工具自带这个，Windows不支持）,会进入一个交互式的环境，如下图：

<img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484282196260.png" width="462"/>

1. 选择使用Android调试插件。执行如下命令：

    ```shell
    platform select remote-android
    ```
    如果提示没有Android，那么你可能需要升级一下你的XCode；只有新版本的lldb才支持Android插件。
2. 连接到lldb-server

    这一步比较简单，但是没有任何官方文档有说明；使用办法是我查阅Android Studio的源码学习到的。如下：
    
    ```shell
    platform connect unix-abstract-connect:///data/local/tmp/debug.sock
    ```
    
    正常情况下你执行lldb-server的那个终端应该有了输出：
    
    <img src="http://7xp3xc.com1.z0.glb.clouddn.com/201601/1484282509260.png" width="430"/>

3. attach到调试进程。首先你需要查出你要调试的那个进程的pid，直接用ps即可；打开一个新的终端执行：

    ```shell
     ~ adb shell ps | grep lldbtest
u0_a53    2242  724   787496 33084 ffffffff b6e0c474 S com.example.weishu.lldbtest
    ```
    
    我要调试的那个进程pid是 `2242`，接下来回到lldb的那个交互式窗口执行：
    
    ```shell
    process attach -p 2242
    ```
    
    如果你的设备没有root，那么这一步就会失败——没有权限去调试一个别的进程；非root设备的调试方法见下文。
    
    至此，调试环境就建立起来了。不需要像gdb那样设置端口转发，lldb的Android调试插件自动帮我们处理好了这些问题。虽然说了这么多，但是你熟练之后真正的步骤只有两步，灰常简单。
    
4. 断点调试

    调试环境建立之后自然就可以进行调试了，如果进行需要学习lldb的使用方法；我这里先演示一下，不关心的可以略过。
    
    1. 首先下一个断点：

        ```shell
        (lldb) br s -n CollectGarbageInternal
Breakpoint 1: where = libart.so`art::gc::Heap::CollectGarbageInternal(art::gc::collector::GcType, art::gc::GcCause, bool), address = 0xb4648c20
        ```
    2. 触发断点之后，查看当前堆栈：

        ```shell
        (lldb) bt
  * thread #8: tid = 2254, 0xb4648c20 libart.so`art::gc::Heap::CollectGarbageInternal(art::gc::collector::GcType, art::gc::GcCause, bool), name = 'GCDaemon', stop reason = breakpoint 1.1
  * frame #0: 0xb4648c20 libart.so`art::gc::Heap::CollectGarbageInternal(art::gc::collector::GcType, art::gc::GcCause, bool)
    frame #1: 0xb464a550 libart.so`art::gc::Heap::ConcurrentGC(art::Thread*) + 52
    frame #2: 0x72b17161 com.example.weishu.lldbtest
        ```
    
    3. 查看寄存器的值

        ```shell
        (lldb) reg read
General Purpose Registers:
        r0 = 0xb4889600
        r1 = 0x00000001
        r2 = 0x00000001
        r3 = 0x00000000
        r4 = 0xb4889600
        r5 = 0xb4835000
        r6 = 0xb47fcfe4  libart.so`art::Runtime::instance_
        r7 = 0xa6714380
        r8 = 0xa6714398
        r9 = 0xb4835000
       r10 = 0x00000000
       r11 = 0xa6714360
       r12 = 0xb47fbb28  libart.so`art::Locks::logging_lock_
        sp = 0xa6714310
        lr = 0xb464a551  libart.so`art::gc::Heap::ConcurrentGC(art::Thread*) + 53
        pc = 0xb4648c20  libart.so`art::gc::Heap::CollectGarbageInternal(art::gc::collector::GcType, art::gc::GcCause, bool)
      cpsr = 0x20000030
      ```
      
      我们可以看到寄存器 `r0`的值为 `0xb4889600`，这个值就是 `CollectGarbageInternal`函数的第一个参数，this指针，也就是当前Heap对象的地址。在ARM下，r0~r4存放函数的参数，超过四个的参数放在栈上，具体如何利用这些寄存器的信息需要了解一些ARM汇编知识。
    4. 查看运行的汇编代码

        ```shell
        (lldb) di -p
libart.so`art::gc::Heap::CollectGarbageInternal:
->  0xb4648c20 <+0>:  push.w {r4, r5, r6, r7, r8, r9, r10, r11, lr}
    0xb4648c24 <+4>:  subw   sp, sp, #0x52c
    0xb4648c28 <+8>:  ldr.w  r9, [pc, #0xa9c]
    0xb4648c2c <+12>: add    r4, sp, #0x84
        ```

#### 没有root设备的调试办法

如果没有root权限，那么我可以借助run-as命令。run-as可以让我们以某一个app的身份执行命令——如果我们以被调试的那个app的身份进行attach，自然是可以成功的。

假设被调试的app包名为 `com.example.lldb`，那么首先想办法把 `lldb-server`这个文件推送到这个app自身的目录：

1. `adb push`直接这么做不太方便（还需要知道userid)，我们先push到 /data/local/tmp/

    `adb push lldb-server /data/local/tmp/`

2. 然后执行adb shell，连接到Android shell，执行
    
    `run-as com.example.lldb`。

3. 拷贝这个文件到本App的目录，并修改权限；（由于有的手机没有cp命令，改用cat)

    ```shell
    cat /data/local/tmp/lldb-server > lldb-server
    chmod 777 lldb-server
    ```
4. 运行lldb-server

    `lldb-server platform --listen unix-abstract:///data/local/tmp/debug.sock`
    
接下来的步骤就与上面root设备的调试过程完全一样了 :)

1. [Android Studio你不知道的调试技巧](http://weishu.me/2015/12/21/android-studio-debug-tips-you-may-not-know/)
2. [如何调试Android Framework][1]
3. [如何调试Android Framework Native](#)


## FAQ

1. 如何获取system.img?
  由于github限制文件大小，而单个system.img 为500+M，因此我使用zip对这个文件进行了分段压缩，只需要使用：
  
  ```shell
  cd image
  cat * > system.zip
  ```
  
  即可得到system.img的zip压缩格式，解压缩即可。
 
2. 如果获取带调试符号信息的libart.so
  
  直接进入symbols目录，解压libart.so.zip即可。
  
[1]: http://weishu.me/2016/05/30/how-to-debug-android-framework/
[2]: https://github.com/tiann/android-native-debug

