#include <iostream>
#include <memory>
#include <chrono>

#include <cmath>


#include <pulse/pulseaudio.h>

static pa_stream *stream = nullptr;

static const pa_channel_map *map = nullptr;

float level = 0.0F;

float volume = 0;

void subscribe_success_callback(pa_context *c, int success, void *);
void context_get_server_info_callback(pa_context *c, const pa_server_info *si, void *);

void context_state_callback(pa_context *c, void *)
{
  switch (pa_context_get_state(c)) {
  case PA_CONTEXT_UNCONNECTED:
    std::cout << "PA_CONTEXT_UNCONNECTED\n";
    break;
  case PA_CONTEXT_CONNECTING:
    std::cout << "PA_CONTEXT_CONNECTING\n";
    break;
  case PA_CONTEXT_AUTHORIZING:
    std::cout << "PA_CONTEXT_AUTHORIZING\n";
    break;
  case PA_CONTEXT_SETTING_NAME:
    std::cout << "PA_CONTEXT_SETTING_NAME\n";
    break;
  case PA_CONTEXT_READY:
    std::cout << "PA_CONTEXT_READY\n";
    pa_operation_unref(pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, subscribe_success_callback, nullptr));
    pa_operation_unref(pa_context_get_server_info(c, context_get_server_info_callback, nullptr));
    break;
  case PA_CONTEXT_FAILED:
    std::cout << "PA_CONTEXT_FAILED\n";
    break;
  case PA_CONTEXT_TERMINATED:
    std::cout << "PA_CONTEXT_TERMINATED\n";
    break;
  default:
    std::cout << "WTF? " << pa_context_get_state(c) << '\n';
  }
}

static void stream_read_callback(pa_stream *s, size_t size, void *)
{
  const void *p = nullptr;

  if (pa_stream_peek(s, &p, &size) < 0) {
    std::cerr << "pa_stream_peak() failed\n";
    return;
  }

  const float *d = reinterpret_cast<const float *>(p);
  size_t samples = size / sizeof(float);
  const uint8_t nchan = map->channels;

  float levels[PA_CHANNELS_MAX];

  for (uint8_t c = 0; c < nchan; c++) levels[c] = 0;

  while (samples >= nchan) {
    for (uint8_t c = 0; c < nchan; c++) {
      const float v = std::fabs(d[c]);
      if (v > levels[c]) levels[c] = v;
    }

    d += nchan;
    samples -= nchan;
  }

  float new_level = 0.0F;
  int div = 0;
  for (uint8_t c = 0; c < nchan; c++) {
    new_level += levels[c];

    if (levels[c] != 0.0F) div++;
  }

  if (div == 0) div = 1;

  new_level = new_level / static_cast<float>(div);

  if (volume <= 0.0F) volume = 1.0F;
  new_level *= 1.0F / volume;

  if (new_level > level) level = new_level;

  pa_stream_drop(s);
}


static void stream_state_callback(pa_stream *s, void *)
{
  switch (pa_stream_get_state(s)) {
  case PA_STREAM_UNCONNECTED:
    std::cout << "PA_STREAM_UNCONNECTED\n";
    break;

  case PA_STREAM_CREATING:
    std::cout << "PA_STREAM_CREATING\n";
    break;

  case PA_STREAM_READY:
    std::cout << "PA_STREAM_READY\n";
    map = pa_stream_get_channel_map(s);

    {
    auto format_info = pa_stream_get_format_info(s);
    std::cout << pa_encoding_to_string(format_info->encoding) << '\n';
    void *state = nullptr;
    const char *key = nullptr;
    while((key = pa_proplist_iterate(format_info->plist, &state)) != nullptr)
      std::cout << key << ": " << pa_proplist_gets(format_info->plist, key) << '\n';
    }

    break;

  case PA_STREAM_FAILED:
    std::cout << "PA_STREAM_FAILED\n";
    break;

  case PA_STREAM_TERMINATED:
    std::cout << "PA_STREAM_TERMINATED\n";
    break;
  }
}


static void create_stream(pa_context *c,
  const char *name,
  const char * /* unused */,
  const pa_sample_spec &ss,
  const pa_channel_map &cmap)
{
  std::cout << "Creating stream on device: " << name << '\n';

  pa_sample_spec nss;

  nss.format = PA_SAMPLE_FLOAT32;
  nss.rate = ss.rate;
  nss.channels = ss.channels;

  stream = pa_stream_new(c, "Connecting dots", &nss, &cmap);
  pa_stream_set_state_callback(stream, stream_state_callback, nullptr);
  pa_stream_set_read_callback(stream, stream_read_callback, nullptr);
  pa_stream_connect_record(stream, name, nullptr, PA_STREAM_PEAK_DETECT);
}


static void
  context_get_sink_info_callback(pa_context *c, const pa_sink_info *si, int is_last, void *)
{
  if (is_last < 0) {
    std::cerr << "Failed to get sink information\n";
    return;
  }

  if (!si) return;

  pa_volume_t vol = pa_cvolume_avg(&si->volume);
  volume = static_cast<float>(pa_sw_volume_to_linear(vol));

  create_stream(c, si->monitor_source_name, si->description, si->sample_spec, si->channel_map);
}


void context_get_server_info_callback(pa_context *c, const pa_server_info *si, void *)
{
  if (!si) {
    std::cerr << "Failed to get server information\n";
    return;
  }

  if (!si->default_sink_name) {
    std::cerr << "No default sink set.\n";
    return;
  }

  pa_operation_unref(pa_context_get_sink_info_by_name(
    c, si->default_sink_name, context_get_sink_info_callback, nullptr));
}

void context_get_sink_info_callback_volume(pa_context *, const pa_sink_info *si, int is_last, void *)
{
  std::cout << "GET SINK INFO CALLBACK VOLUME\n";

  if (is_last < 0) {
    std::cerr << "Failed to get sink information\n";
    return;
  }

  if (!si) return;

  pa_volume_t vol = pa_cvolume_avg(&si->volume);
  volume = static_cast<float>(pa_sw_volume_to_linear(vol));
}

void context_get_server_info_callback_volume(pa_context *c, const pa_server_info *si, void *)
{
  std::cout << "GET SERVER INFO CALLBACK VOLUME\n";

  if (!si) {
    std::cerr << "Failed to get server information\n";
    return;
  }

  if (!si->default_sink_name) {
    std::cerr << "No default sink set.\n";
    return;
  }

  pa_operation_unref(pa_context_get_sink_info_by_name(
    c, si->default_sink_name, context_get_sink_info_callback_volume, nullptr));
}

void subscription_event_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *) {
  std::cout << "Event received: " << t <<  " idx: " << idx << '\n';

  pa_operation_unref(pa_context_get_server_info(c, context_get_server_info_callback_volume, nullptr));
}

void subscribe_success_callback(pa_context *c, int success, void *)
{
  std::cout << "SUBSCRIBE SUCCESS CALLBACK success=" << success << '\n';
  if (success){
    pa_context_set_subscribe_callback(c, subscription_event_callback, nullptr);
  }
}
