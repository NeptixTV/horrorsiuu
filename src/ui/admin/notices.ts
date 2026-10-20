// Start-up notices: unseen news messages and available updates.
import { create } from 'zustand';
import { APP_VERSION, compareVersions, fetchFeed, seenMessages, skippedVersion, type FeedMessage, type FeedUpdate } from '../../admin/feed';
import { toast, ui } from '../../state/ui';

interface NoticeState {
  messages: FeedMessage[];
  update: FeedUpdate | null;
  show: 'news' | 'update' | null;
  checking: boolean;
}

export const useNotices = create<NoticeState>(() => ({ messages: [], update: null, show: null, checking: false }));

/** Checks the feed. `manual` also shows old messages / "you're up to date". */
export async function checkNotices(manual = false) {
  const prefs = ui();
  // Settings → Privacy & updates: no request at all when both are switched off
  if (!manual && !prefs.checkUpdates && !prefs.showNews) return;
  useNotices.setState({ checking: true });
  const feed = await fetchFeed();
  useNotices.setState({ checking: false });
  if (!feed) {
    if (manual) toast('Could not reach the update server – are you online?', 'error');
    return;
  }
  const seen = seenMessages();
  const messages = manual ? feed.messages : prefs.showNews ? feed.messages.filter((m) => !seen.has(m.id)) : [];
  const newer = feed.update && compareVersions(feed.update.version, APP_VERSION) > 0 ? feed.update : null;
  const update = newer && (manual || (prefs.checkUpdates && (newer.mandatory || skippedVersion() !== newer.version))) ? newer : null;
  useNotices.setState({ messages, update, show: messages.length ? 'news' : update ? 'update' : null });
  if (manual && !messages.length && !update) toast(`Goofy Studio ${APP_VERSION} is up to date – no news`, 'ok');
}
