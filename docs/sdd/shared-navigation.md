# Shared keyboard navigation

Bluetooth is one sectioned ListView with a stable device-ID cursor. Headings are not targets. Discovery updates reconcile rows in place and preserve focused controls. Movement never dispatches device actions; Enter or l enters controls, and h leaves controls in Vim navigation. Header/footer remain outside the collection. Pairing input suppresses collection commands.

The interaction contract and installed-provider verification order are documented in the
umbrella `docs/shared-keyboard-navigation.md` and provider `holonight-qt/docs/navigation.md`.
Native focus and reachability acceptance remains manual. No deployment is part of this change.

Opening Bluetooth focuses the power switch with a visible keyboard outline. Tab
continues through the collection and its explicit action controls; movement keys
navigate devices without invoking actions. This preserves access to row controls
while ordinary file rows remain part of a single collection Tab stop.
