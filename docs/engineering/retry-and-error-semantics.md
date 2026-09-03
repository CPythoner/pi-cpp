# Retry 与 Error Semantics：容错也是可观察行为

> 建立版本：v0.0.2。  
> 主要代码：`src/ai/retry.*`、`src/ai/openai_provider.*`、`src/ai/http.*`。  
> 兼容来源：pi `v0.80.0` 使用的 `openai-node 6.26.0`。

## 1. 为什么 Retry 需要独立文档

Retry 很容易被当成“内部实现细节”，但对 streaming Provider 来说，它会直接改变：

- 请求实际发送次数；
- 用户等待时间；
- 是否重复执行潜在有副作用的请求；
- 取消被观察的时点；
- 最终 error event；
- 是否出现重复 text/tool-call 事件。

因此长期原则是：

> retry policy 是可观察兼容语义的一部分，不能只按“更健壮”或“更先进”自行优化。

## 2. 固定基线

v0.0.2 OpenAI-compatible path 对齐：

```text
pi v0.80.0
openai-node 6.26.0
```

当前默认：

```text
maxRetries = 0
```

只有调用方显式设置后才允许额外 request attempt。

## 3. Retryable HTTP status

当前兼容集合：

```text
408   Request Timeout
409   Conflict
429   Too Many Requests
5xx   Server Error
```

以下典型状态默认不 retry：

```text
400
401
403
404
```

尤其 401/403 通常是配置/权限问题，重试不会改变结果。

## 4. `x-should-retry` 显式覆盖

如果 response header 中存在：

```text
x-should-retry: true
```

即使 status 默认不可重试，也按 server 指示 retry。

如果：

```text
x-should-retry: false
```

即使 status 默认属于 retryable，也不 retry。

处理 header name 时必须大小写不敏感。

优先级：

```text
x-should-retry explicit directive
        >
default status classification
```

## 5. Transport failure 与 HTTP response failure 不一样

openai-node 的 request retry 主要覆盖：

> 在拿到 HTTP Response 之前发生的 connection/fetch failure。

因此当前判断：

```text
HttpErrorKind::Transport && status == 0
    → request-level retry candidate
```

但如果已经有：

```text
status == 200
```

之后 body stream 断开，不能把整个 request 重新发送。

原因是请求已经获得成功 response，甚至可能已经产生部分流式内容；重放可能造成重复事件或重复副作用。

## 6. Stream started 后禁止 retry

核心条件之一：

```text
streamStarted == true
    → no retry
```

一旦 consumer 已经观察到：

```text
start
text_delta
toolcall_delta
```

重新发请求就会破坏事件流唯一性。

即使新请求最终生成相同文本，consumer 也会看到重复 start/delta。

因此：

> retry 只属于“请求尚未进入可观察 streaming 阶段”的恢复机制。

## 7. Server 指定 delay 的解析顺序

当前顺序：

```text
1. retry-after-ms
2. Retry-After: <seconds>
3. Retry-After: <HTTP-date>
4. exponential backoff + jitter
```

### 7.1 `retry-after-ms`

单位是毫秒。

### 7.2 `Retry-After` 数字

HTTP 标准形式之一，单位秒：

```text
Retry-After: 2
→ 2000ms
```

### 7.3 `Retry-After` HTTP-date

例如：

```text
Retry-After: Wed, 21 Oct 2015 07:28:00 GMT
```

实现不依赖进程 locale/timezone，而是直接解析 IMF-fixdate 并转换为 UTC/Unix time。

如果日期已经过去：

```text
delay = 0
```

## 8. 为什么不使用 `StreamOptions::maxRetryDelay` 截断 OpenAI Retry-After

`StreamOptions` 保留 provider-specific：

```text
maxRetryDelay
```

但 v0.0.2 的 OpenAI-compatible provider**故意忽略它**。

原因不是实现遗漏，而是固定基线 pi v0.80.0 的 OpenAI path 没有把这个字段传入 openai-node retry 行为。

所以：

```text
server Retry-After = 120s
maxRetryDelay = 5s
```

不能为了“体验更好”在当前 OpenAI path 私自截成 5s，否则产生兼容差异。

## 9. Exponential Backoff

server 没指定 delay 时：

```text
retry 0 → 500ms
retry 1 → 1000ms
retry 2 → 2000ms
retry 3 → 4000ms
retry 4+ → 8000ms cap
```

然后应用 jitter。

## 10. Jitter

当前：

```text
jitterFactor = 1.0 - 0.25 * randomUnit
randomUnit ∈ [0, 1]
```

因此实际 delay 位于：

```text
75% ~ 100%
```

base backoff 之间。

测试通过 injectable random source 固定 randomUnit，避免 flaky test。

## 11. Retry Hooks 为什么是 private test seam

Retry 依赖三个非确定因素：

```text
clock
random
sleep
```

如果单测使用真实时间和随机数：

- 测试慢；
- CI flaky；
- 很难验证精确 delay。

因此 private `RetryHooks` 注入：

```text
now()
randomUnit()
sleep()
```

production 用真实实现，tests 用 deterministic implementation。

它是测试 seam，不应因此升级成 public retry strategy API。

## 12. Cancellation during retry sleep

这是 v0.0.2 closeout 特别纠正过的语义。

从纯工程角度看，可以写成：

```text
sleep 可被 CancellationToken 立即唤醒
```

但固定基线 openai-node 6.26.0 的 retry sleep 本身不会被 AbortSignal 提前唤醒。

实际兼容行为：

```text
sleep full delay
      ↓
检查 cancellation
      ↓
如果 cancelled，不发下一次 request
```

所以 pi-cpp production default 使用 `sleep_for()`，sleep 完后才观察 token。

这里体现一个重要原则：

> “更快响应取消”不自动等于“兼容实现”。

如果未来项目决定有意偏离 upstream，应明确记录为本地语义升级，而不是悄悄改掉。

## 13. In-flight cancellation

Retry sleep 的行为不影响正在执行的 HTTP request。

in-flight request cancellation 仍应该：

```text
CancellationToken requested
        ↓
transport abort
        ↓
HttpErrorKind::Cancelled
        ↓
EvError / StopReason::Aborted
```

这与“backoff sleep 是否可中断”是两条不同路径。

## 14. Error 分类

Provider runtime 至少区分：

```text
Cancelled
Transport
ConsumerAborted
HTTP status error
Protocol error
Provider semantic error
Configuration error
```

这样 retry policy 只处理自己负责的错误类型，而不是看到任何 error 都重试。

### Configuration

如：

```text
missing API key
empty base URL
```

直接 terminal error，不 retry。

### HTTP status

按 status/header policy 判断。

### Transport

只有 response 尚不存在时可能 request retry。

### Protocol

如 malformed SSE JSON、missing finish_reason：已经进入 protocol processing，不重放请求。

### Cancellation

terminal aborted，不 retry。

## 15. 成功 HTTP + body failure 的事件顺序

pi 的可观察行为是：拿到成功 Response 后会发 `start`，之后才消费 body。

因此如果：

```text
HTTP 200 headers received
body before first SSE fails
```

pi-cpp 必须保持：

```text
start
error
```

而不是：

```text
error
```

也不能重试。

这说明 error mapping 不能只看“有没有收到正文”，还要考虑 HTTP lifecycle 已经走到了哪个可观察阶段。

## 16. 为什么 Retry 与 Decoder 分开

Decoder 不知道 request 是否是第几次尝试。

它只处理：

```text
成功 transport 提供的 stream bytes
```

Retry 属于 ProviderCore / transport lifecycle：

```text
request attempt
    ↓
response?
    ↓
retry decision
    ↓
成功后才持续驱动 decoder
```

把 retry 放进 decoder 会混淆：

- 网络级重放；
- semantic stream state；
- terminal event state。

## 17. 修改 Retry 时的 focused test checklist

至少覆盖：

```text
408 retry
409 retry
429 retry
500/503 retry
401 no retry
403 no retry
x-should-retry true
x-should-retry false
retry-after-ms
Retry-After seconds
Retry-After HTTP-date
expired HTTP-date
exponential backoff
jitter boundaries
maxRetries exhausted
transport status=0 retry
transport status=200 no retry
streamStarted no retry
cancelled no retry
consumer aborted no retry
sleep cancellation timing
```

Provider integration test还应验证：

```text
actual attempt count
actual sleep count
no duplicate events
retry then success
```

## 18. 设计结论

Retry/Error 的长期原则：

1. **Retry 只处理明确可重放的 request-level failure。**
2. **一旦进入可观察 stream，就不能重放整个请求。**
3. **server retry directive 优先于本地默认分类。**
4. **server delay 优先于 exponential backoff。**
5. **取消时序也是兼容行为，不能凭“更合理”私自改变。**
6. **error 必须先分类，再决定 retry；不能把 retry 当通用 catch-all。**
