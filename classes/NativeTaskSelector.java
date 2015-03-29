package com.stackoverflow.interrupt;

import java.io.IOException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.spi.AbstractSelectableChannel;
import java.nio.channels.spi.AbstractSelector;
import java.util.Set;

/**
 * NativeTaskSelector isn't a "real" Selector, it's a dummy object because we
 * want to hook into the goodness of AbstractSelector.  AbstractSelector uses
 * some cunning private goop (sun.misc.SharedSecrets) to hook into the current
 * thread object and receive notifications when the thread is interrupted.
 * Since that's what we're after, we implement AbstractSelector simply to
 * receive that notification.
 */
class NativeTaskSelector extends AbstractSelector {

	protected NativeTaskSelector(NativeTask task_) {
		super(null);
		task = task_;
	}

	public void registerStart() { begin(); }
	public void registerEnd() { end(); }

	final private NativeTask task;

	@Override
	protected void implCloseSelector() throws IOException {
	}

	@Override
	protected SelectionKey register(AbstractSelectableChannel arg0, int arg1,
			Object arg2) {
		throw new UnsupportedOperationException();
	}

	@Override
	public Set<SelectionKey> keys() {
		throw new UnsupportedOperationException();
	}

	@Override
	public int select() throws IOException {
		throw new UnsupportedOperationException();
	}

	@Override
	public int select(long arg0) throws IOException {
		throw new UnsupportedOperationException();
	}

	@Override
	public int selectNow() throws IOException {
		throw new UnsupportedOperationException();
	}

	@Override
	public Set<SelectionKey> selectedKeys() {
		throw new UnsupportedOperationException();
	}

	@Override
	public Selector wakeup() {
		System.out.println("Received notification of Thread interrupt");
		task.wakeupTask();
		return this;
	}

}
