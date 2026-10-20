// Keeps one broken panel or window from taking the whole studio down: the
// error is logged (Settings → Audio → Diagnostics) and the panel offers a
// reload instead of a blank screen.
import { Component, type ReactNode } from 'react';
import { diagnostics } from '../../audio/diagnostics';

interface Props {
  name: string;
  children: ReactNode;
  /** compact inline message instead of a panel */
  inline?: boolean;
  onClose?: () => void;
  /** wraps the error message (e.g. in a window frame so it can still be moved / closed) */
  frame?: (content: ReactNode) => ReactNode;
}

interface State {
  error: Error | null;
}

export class ErrorBoundary extends Component<Props, State> {
  state: State = { error: null };

  static getDerivedStateFromError(error: Error): State {
    return { error };
  }

  componentDidCatch(error: Error) {
    diagnostics.log('error', 'ui-crash', `${this.props.name}: ${error.message}`);
    console.error(error);
  }

  render() {
    const { error } = this.state;
    if (!error) return this.props.children;
    const content = (
      <div className={`crash ${this.props.inline ? 'inline' : ''}`} role="alert">
        <b>{this.props.name} ran into a problem.</b>
        <span className="crash-msg">{error.message}</span>
        <span className="crash-actions">
          <button className="btn small" onClick={() => this.setState({ error: null })}>Reload panel</button>
          {this.props.onClose && <button className="btn small ghost" onClick={this.props.onClose}>Close</button>}
        </span>
        <span className="crash-hint">Your project is safe. The details are in Settings → Audio → Diagnostics.</span>
      </div>
    );
    return this.props.frame ? this.props.frame(content) : content;
  }
}
